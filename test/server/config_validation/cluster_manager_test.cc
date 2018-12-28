#include "envoy/upstream/resource_manager.h"
#include "envoy/upstream/upstream.h"

#include "common/api/api_impl.h"
#include "common/http/context_impl.h"
#include "common/ssl/context_manager_impl.h"

#include "server/config_validation/cluster_manager.h"

#include "test/mocks/access_log/mocks.h"
#include "test/mocks/event/mocks.h"
#include "test/mocks/http/mocks.h"
#include "test/mocks/local_info/mocks.h"
#include "test/mocks/network/mocks.h"
#include "test/mocks/runtime/mocks.h"
#include "test/mocks/secret/mocks.h"
#include "test/mocks/server/mocks.h"
#include "test/mocks/thread_local/mocks.h"
#include "test/mocks/upstream/mocks.h"
#include "test/test_common/simulated_time_system.h"
#include "test/test_common/utility.h"

namespace Envoy {
namespace Upstream {

TEST(ValidationClusterManagerTest, MockedMethods) {
  Stats::IsolatedStoreImpl stats_store;
  Api::ApiPtr api(Api::createApiForTest(stats_store));
  NiceMock<Runtime::MockLoader> runtime;
  Event::SimulatedTimeSystem time_system;
  NiceMock<ThreadLocal::MockInstance> tls;
  NiceMock<Runtime::MockRandomGenerator> random;
  testing::NiceMock<Secret::MockSecretManager> secret_manager;
  auto dns_resolver = std::make_shared<NiceMock<Network::MockDnsResolver>>();
  Ssl::ContextManagerImpl ssl_context_manager{time_system};
  NiceMock<Event::MockDispatcher> dispatcher;
  LocalInfo::MockLocalInfo local_info;
  NiceMock<Server::MockAdmin> admin;
  Http::ContextImpl http_context;

  ValidationClusterManagerFactory factory(runtime, stats_store, tls, random, dns_resolver,
                                          ssl_context_manager, dispatcher, local_info,
                                          secret_manager, *api, http_context);

  AccessLog::MockAccessLogManager log_manager;
  const envoy::config::bootstrap::v2::Bootstrap bootstrap;
  ClusterManagerPtr cluster_manager = factory.clusterManagerFromProto(
      bootstrap, stats_store, tls, runtime, random, local_info, log_manager, admin);
  EXPECT_EQ(nullptr, cluster_manager->httpConnPoolForCluster("cluster", ResourcePriority::Default,
                                                             Http::Protocol::Http11, nullptr));
  Host::CreateConnectionData data = cluster_manager->tcpConnForCluster("cluster", nullptr, nullptr);
  EXPECT_EQ(nullptr, data.connection_);
  EXPECT_EQ(nullptr, data.host_description_);

  Http::AsyncClient& client = cluster_manager->httpAsyncClientForCluster("cluster");
  Http::MockAsyncClientStreamCallbacks stream_callbacks;
  EXPECT_EQ(nullptr, client.start(stream_callbacks, Http::AsyncClient::StreamOptions()));
}

} // namespace Upstream
} // namespace Envoy
