#pragma once

#include "aex/ipc/event.hpp"
#include "aex/mem.hpp"
#include "aex/net.hpp"
#include "aex/rwspinlock.hpp"
#include "aex/spinlock.hpp"

#include <stddef.h>
#include <stdint.h>

namespace NetStack {
    namespace TCPConfig {
        static constexpr auto FLAG_PRINT = false;
    }

    static constexpr auto TCP_RETRANSMIT_INTERVAL = 2ul * 1000000000;
    static constexpr auto TCP_KEEPALIVE_INTERVAL  = 60ul * 1000000000;
    static constexpr auto TCP_CONNECTION_TIMEOUT  = 7ul * 1000000000;
    static constexpr auto TCP_ARP_UPDATE_INTERVAL = 15ul * 1000000000;

    static constexpr auto TCP_RETRIES        = 5;
    static constexpr auto TCP_TX_BUFFER_SIZE = 4096;
    static constexpr auto TCP_RX_BUFFER_SIZE = 8192 + 64;
    static constexpr auto TCP_MIN_MSS        = 256;
    static constexpr auto TCP_PROBE_SLACK    = 64;

    enum tcp_state_t {
        TCP_LISTEN,
        TCP_SYN_SENT,
        TCP_SYN_RECEIVED,
        TCP_ESTABLISHED,
        TCP_FIN_WAIT_1,
        TCP_FIN_WAIT_2,
        TCP_CLOSE_WAIT,
        TCP_CLOSING,
        TCP_LAST_ACK,
        TCP_TIME_WAIT,
        TCP_CLOSED,
    };

    enum tcp_flags_t {
        TCP_FIN = 0x01,
        TCP_SYN = 0x02,
        TCP_RST = 0x04,
        TCP_PSH = 0x08,
        TCP_ACK = 0x10,
        TCP_URG = 0x20,
    };

    enum tcp_kind_t {
        TCP_OPT_END  = 0x00,
        TCP_OPT_NOOP = 0x01,
        TCP_OPT_MSS  = 0x02,
    };

    char* tcp_debug_serialize_flags(char* buffer, int flags);

    class TCPProtocol;

    struct tcp_block {
        volatile tcp_state_t state = TCP_CLOSED;

        volatile uint32_t snd_una; // position
        volatile uint32_t snd_nxt; // position
        volatile uint16_t snd_wnd; // value
        volatile uint16_t snd_mss; // value

        volatile uint32_t rcv_nxt; // position
        // uint16_t rcv_wnd; // value

        bool sending_comp_ack      = false;
        bool notify_of_window_size = false;
        bool closing               = false;

        volatile bool send_shut    = false;
        volatile bool receive_shut = false;
    };

    struct tcp_listen_entry {
        AEX::Net::ipv4_addr source_address;
        uint16_t            source_port = 0;

        tcp_block block;

        uint16_t data_len    = 0;
        uint8_t* data_buffer = nullptr;

        volatile uint8_t refs = 0;

        ~tcp_listen_entry() {
            if (data_buffer)
                delete data_buffer;

            data_buffer = nullptr;
        }
    };

    class TCPSocket : public AEX::Net::Socket {
        public:
        AEX::Net::Socket_SP* thisSmartPtr =
            new AEX::Net::Socket_SP(this, new AEX::Mem::sp_shared(0));

        AEX::Net::ipv4_addr source_address;
        uint16_t            source_port = 0;

        AEX::Net::ipv4_addr destination_address;
        uint16_t            destination_port = 0;

        TCPSocket() {}
        TCPSocket(TCPSocket* parent, tcp_listen_entry* listen_entry);

        ~TCPSocket();

        AEX::error_t connect(const AEX::Net::sockaddr* addr);
        AEX::error_t bind(const AEX::Net::sockaddr* addr);
        AEX::error_t listen(int backlog);

        AEX::optional<AEX::Net::Socket_SP> accept();

        AEX::optional<size_t> sendTo(const void* buffer, size_t len, int flags,
                                     const AEX::Net::sockaddr* dst_addr);
        AEX::optional<size_t> receiveFrom(void* buffer, size_t len, int flags,
                                          AEX::Net::sockaddr* src_addr);

        AEX::error_t shutdown(int how);
        AEX::error_t close();

        private:
        struct segment {
            uint32_t seq = 0;
            uint32_t ack = 0;

            bool     acked    = false;
            uint32_t ack_with = 0;

            tcp_flags_t flags;
            uint64_t    retransmit_at = 0;
            uint8_t     retries       = 0;

            int      header_len = 8;
            int      len        = 0;
            uint8_t* data       = nullptr;

            ~segment() {
                if (data)
                    delete data;

                data = nullptr;
            }
        };

        struct segment_retry {
            AEX::Net::Socket_SP socket;

            AEX::Net::ipv4_addr addr;
            uint16_t            port;

            segment  seg;
            uint16_t async_id;

            segment_retry(AEX::Net::Socket_SP m_socket, AEX::Net::ipv4_addr m_addr, uint16_t m_port,
                          segment* m_seg)
                : socket(m_socket), seg(*m_seg) {
                addr = m_addr;
                port = m_port;

                seg.data = (uint8_t*) AEX::Mem::Heap::malloc(m_seg->len);
                AEX::memcpy(seg.data, m_seg->data, m_seg->len);

                async_id = ((TCPSocket*) m_socket.get())->m_async_id;
            }
        };

        AEX::Spinlock m_lock;

        AEX::IPC::Event m_tx_event = {};
        AEX::IPC::Event m_rx_event = {};

        AEX::Mem::CircularBuffer m_tx_buffer = AEX::Mem::CircularBuffer(TCP_TX_BUFFER_SIZE);
        AEX::Mem::CircularBuffer m_rx_buffer = AEX::Mem::CircularBuffer(TCP_RX_BUFFER_SIZE);

        AEX::Mem::Vector<segment*, 16, 16>             m_retransmission_queue;
        AEX::Mem::Vector<tcp_listen_entry, 16, 16>*    m_listen_queue = nullptr;
        AEX::Mem::Vector<AEX::Net::Socket_SP, 16, 16>* m_accept_queue = nullptr;

        tcp_block m_block;
        int       m_listen_backlog;

        uint16_t m_async_id = 0;

        static void readOptions(tcp_block* block, const uint8_t* buffer, uint16_t hdr_len);

        void clearState();

        void tick();

        void syn();
        void ack();
        void ack(uint32_t manual);
        void rst();
        void fin();

        bool reset();
        void windowUpdate();

        int               getWindow();
        tcp_listen_entry* tryCreateListenEntry(AEX::Net::ipv4_addr addr, uint16_t port);
        tcp_listen_entry* getListenEntry(AEX::Net::ipv4_addr addr, uint16_t port);
        void              removeEntry(AEX::Net::ipv4_addr addr, uint16_t port);

        bool sendSegment(segment* seg);
        bool sendSegment(segment* seg, AEX::Net::ipv4_addr addr, uint16_t port);

        void packetReceived(AEX::Net::ipv4_addr src, uint16_t src_port, const uint8_t* buffer,
                            uint16_t len);
        void packetReceivedListen(AEX::Net::ipv4_addr src, uint16_t src_port, const uint8_t* buffer,
                                  uint16_t len);

        void closeInternal();

        void refresh();
        void package();
        void retransmit();

        friend class TCPProtocol;
    };
}