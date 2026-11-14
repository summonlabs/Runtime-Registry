// Regression tests: a rejected instance registration or supersession changes
// nothing, and supersession never installs an older generation.
#include <runtimeregistry/registry.hpp>
#include "test_util.hpp"

using namespace runtimeregistry;

namespace {

struct Setup {
  Registry reg;
  AuthorityEnvelope env;
  WorkerBootId boot;
  EndpointId endpoint{400};

  Setup() {
    reg.begin_coordinator_epoch();
    boot = reg.adopt_worker(WorkerId(1));
    env.epoch = reg.authority().epoch;
    env.boot = boot;
    ServiceDescriptor svc;
    svc.service_id = ServiceId(10);
    svc.kind = ServiceKind::MODEL_SERVER;
    svc.name = "inference";
    svc.generation = ServiceGeneration(1);
    svc.version = SemanticVersion::parse("1.0.0").value();
    reg.register_service(svc, env);
    EndpointDescriptor ep;
    ep.endpoint_id = endpoint;
    ep.generation = EndpointGeneration(1);
    ep.protocol = ProtocolId(1);
    ep.locator.text = "127.0.0.1:31817";
    ep.transport = TransportKind::TCP;
    ep.port = 31817;
    ep.health = Health::HEALTHY;
    ep.reachability = Reachability::REACHABLE;
    ep.freshness = Freshness::CURRENT;
    ep.provenance = make_provenance(EvidenceKind::MEASURED, "probe", 0);
    reg.register_endpoint(ep, env);
  }

  ServiceInstance instance(std::uint64_t gen, std::vector<EndpointId> eps) const {
    ServiceInstance inst;
    inst.service_id = ServiceId(10);
    inst.service_generation = ServiceGeneration(1);
    inst.instance_generation = ServiceInstanceGeneration(gen);
    inst.node = NodeId(1);
    inst.worker = WorkerId(1);
    inst.boot = boot;
    inst.endpoints = std::move(eps);
    inst.health = Health::HEALTHY;
    inst.readiness = Readiness::READY;
    inst.freshness = Freshness::CURRENT;
    inst.reachability = Reachability::REACHABLE;
    inst.version = SemanticVersion::parse("1.0.0").value();
    inst.provenance = make_provenance(EvidenceKind::REPORTED, "worker", 0);
    inst.lifecycle = Lifecycle::REGISTERING;
    return inst;
  }
};

// A registration rejected for an unregistered endpoint leaves the current
// instance current and its endpoint bound to it.
void test_rejected_registration_changes_nothing() {
  Setup s;
  const ServiceInstanceId first = s.reg.register_instance(s.instance(1, {s.endpoint}), s.env);
  const Lifecycle before = s.reg.find_instance(first)->lifecycle;
  CHECK_THROWS(s.reg.register_instance(s.instance(2, {s.endpoint, EndpointId(999)}), s.env),
               RegistryError, ErrorKind::UNKNOWN_IDENTITY);
  CHECK(s.reg.find_instance(first)->lifecycle == before);
  CHECK(s.reg.find_instance(first)->lifecycle != Lifecycle::SUPERSEDED);
  CHECK(s.reg.find_endpoint(s.endpoint)->service_instance == first);
}

// supersede() rejects an older replacement generation and a replacement whose
// registration fails, leaving the old instance as it was.
void test_supersede_rejects_stale_or_invalid_replacement() {
  Setup s;
  const ServiceInstanceId cur = s.reg.register_instance(s.instance(5, {s.endpoint}), s.env);
  const Lifecycle before = s.reg.find_instance(cur)->lifecycle;
  CHECK_THROWS(s.reg.supersede(cur, s.instance(3, {s.endpoint}), s.env),
               RegistryError, ErrorKind::STALE_INSTANCE_GENERATION);
  CHECK(s.reg.find_instance(cur)->lifecycle == before);
  CHECK_THROWS(s.reg.supersede(cur, s.instance(6, {EndpointId(999)}), s.env),
               RegistryError, ErrorKind::UNKNOWN_IDENTITY);
  CHECK(s.reg.find_instance(cur)->lifecycle == before);
  // A valid newer replacement supersedes it.
  s.reg.supersede(cur, s.instance(7, {s.endpoint}), s.env);
  CHECK(s.reg.find_instance(cur)->lifecycle == Lifecycle::SUPERSEDED);
}

}  // namespace

void test_regression_suite();
void test_regression_suite() {
  test_rejected_registration_changes_nothing();
  test_supersede_rejects_stale_or_invalid_replacement();
}
RR_REGISTER(test_regression_suite);
int main() { return rr_test::run_all("regression"); }
