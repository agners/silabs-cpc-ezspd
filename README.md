# CPC Zigbee NCP to TCP/IP socket gateway

This repository implements a gateway transferring EmberZNet Protocol (EZSP)
frames received through the CPC (Co-Processor Communication Protocol) to
a TCP/IP socket. The bare EZSP frames are encapsulated using Silicon Labs
ASH protocol. This allows to use Zigbee applications like zigpy's bellow
with the regular "socket://" protocol.

This is useful for Silicon Labs radio running the Zigbee NCP + OpenThread RCP
(CPC-UART) firmware (zigbee_ncp-ot_rcp-uart).

Note: The ASH protocol implementation is very minimal (incomplete)! E.g. there
is no frame retransmition implemented, and frame acks are simply ignored. This
likely works flawless as the underlying transport protocols (TCP/IP and CPC)
are reliable.
