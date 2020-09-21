#include <nano/node/testing.hpp>
#include <nano/node/transport/udp.hpp>
#include <nano/test_common/testutil.hpp>

#include <gtest/gtest.h>

#include <boost/iostreams/stream_buffer.hpp>
#include <boost/range/join.hpp>
#include <boost/thread.hpp>

using namespace std::chrono_literals;

TEST (network, replace_port)
{
	nano::system system;
	nano::node_flags node_flags;
	node_flags.disable_udp = false;
	node_flags.disable_ongoing_telemetry_requests = true;
	node_flags.disable_initial_telemetry_requests = true;
	auto node0 = system.add_node (node_flags);
	ASSERT_EQ (0, node0->network.size ());
	auto node1 (std::make_shared<nano::node> (system.io_ctx, nano::get_available_port (), nano::unique_path (), system.alarm, system.logging, system.work, node_flags));
	node1->start ();
	system.nodes.push_back (node1);
	auto wrong_endpoint = nano::endpoint (node1->network.endpoint ().address (), 23000);
	auto channel0 (node0->network.udp_channels.insert (wrong_endpoint, node1->network_params.protocol.protocol_version));
	ASSERT_NE (nullptr, channel0);
	node0->network.udp_channels.modify (channel0, [&node1](std::shared_ptr<nano::transport::channel> channel_a) {
		channel_a->set_node_id (node1->node_id.pub);
	});
	auto peers_list (node0->network.list (std::numeric_limits<size_t>::max ()));
	ASSERT_EQ (peers_list[0]->get_node_id (), node1->node_id.pub);
	auto channel1 (std::make_shared<nano::transport::channel_udp> (node0->network.udp_channels, node1->network.endpoint (), node1->network_params.protocol.protocol_version));
	ASSERT_EQ (node0->network.udp_channels.size (), 1);
	// On handshake, the channel is replaced
	ASSERT_NO_ERROR (system.poll_until_true (5s, [&]() {
		node0->network.send_keepalive (channel1);
		return !node0->network.udp_channels.channel (wrong_endpoint) && node0->network.udp_channels.channel (node1->network.endpoint ());
	}));
}
