idf_component_register(
  SRCS
    "main.c"
    "config/node_identity.c"
    "components/digital_output.c"
    "delivery/delivery_tracker.c"
    "network/espnow_transport.c"
    "routing/relay_forwarder.c"
    "protocol/network_message.c"
    "topology/topology_store.c"
  INCLUDE_DIRS
    "."
    "config"
    "components"
    "delivery"
    "network"
    "protocol"
    "routing"
    "topology"
)





