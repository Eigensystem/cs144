#ifndef SPONGE_LIBSPONGE_TCP_FSM_HH
#define SPONGE_LIBSPONGE_TCP_FSM_HH

#include "tcp_config.hh"
#include "tcp_receiver.hh"
#include "tcp_sender.hh"
#include "tcp_state.hh"
#include <string>

class TCPConnection;

enum class TState {
  CLOSED,
  LISTEN,
  SYN_SENT,
  SYN_RECEIVED,
  ESTABLISHED,
  CLOSE_WAIT,
  LAST_ACK,
  FIN_WAIT_1,
  FIN_WAIT_2,
  CLOSING,
  TIME_WAIT,
  RESET,
};


class TCP_State_Machine {
  private:
    TState _state{TState::CLOSED};
    bool _active{0};

  public:
    TCP_State_Machine() = default;
    TCP_State_Machine(TState state) : _state(state) {}
    TCP_State_Machine(TCP_State_Machine &&other) = default;
    TCP_State_Machine &operator=(TCP_State_Machine &&other) = default;
    
    TState state() const { return _state; };
    
    bool operator==(const TState state) const;
    bool operator!=(const TState state) const;
    bool active() const { return _active; }
    void send_segment(TCPConnection &conn, bool ack);

//! 3-way handshake
    // just start listening, do not send SYN
    void ClosedToListen();
    // when connect, send SYN to remote
    void ClosedToSYNsent(TCPConnection &conn);
    // received SYN from remote
    void ListenToSYNrecv(TCPConnection &conn, const TCPSegment &seg);
    // received SYN/ACK to SYN
    void SYNsentToEst(TCPConnection &conn, const TCPSegment &seg);
    // received ACK to SYN/ACK
    void SYNrecvToEst(TCPConnection &conn, const TCPSegment &seg);
    // closed after sending SYN/ACK before receiving SYN from remote
    void SYNrecvToFINwait1();
    
//! Passive Close
    // received FIN from remote before sending FIN to remote
    void EstToCloseWait(TCPConnection &conn, const TCPSegment &seg);
    // send FIN after receiving FIN from remote
    void CloseWaitToLastACK();
    // close after receiving ACK to FIN from remote
    void LastACKToClosed(TCPConnection &conn, const TCPSegment &seg);

//! Active Close
    // send FIN to remote before receiving FIN from remote
    void EstToFINwait1();
    // received ACK to FIN, remote can still send msg
    void FINwait1ToFINwait2(TCPConnection &conn, const TCPSegment &seg);
    // received FIN from remote after sending FIN but has not been ACKed yet
    void FINwait1ToClosing(TCPConnection &conn, const TCPSegment &seg);
    // received ACK/FIN from remote after sending FIN
    void FINwait1ToTimeWait(TCPConnection &conn, const TCPSegment &seg);
    // received FIN from remote after sending FIN to remote and been ACKed
    void FINwait2ToTimeWait(TCPConnection &conn, const TCPSegment &seg);
    // close after timeout
    void TimeWaitToClosed();

//! RESET
    void ToReset(TCPConnection &conn);
};

// std::ostream &operator<<(std::ostream &os, const TCP_State_Machine &item) {
//   std::string state_name;
//   if (item.state()==TState::CLOSED)
//     state_name = "CLOSED";
//   else if (item.state()==TState::LISTEN)
//     state_name = "LISTEN";
//   else if (item.state()==TState::SYN_SENT)
//     state_name = "SYN_SENT";
//   else if (item.state()==TState::SYN_RECEIVED)
//     state_name = "SYN_RECEIVED";
//   else if (item.state()==TState::ESTABLISHED)
//     state_name = "ESTABLISHED";
//   else if (item.state()==TState::CLOSE_WAIT)
//     state_name = "CLOSE_WAIT";
//   else if (item.state()==TState::LAST_ACK)
//     state_name = "LAST_ACK";
//   else if (item.state()==TState::FIN_WAIT_1)
//     state_name = "FIN_WAIT_1";
//   else if (item.state()==TState::FIN_WAIT_2)
//     state_name = "FIN_WAIT_2";
//   else if (item.state()==TState::CLOSING)
//     state_name = "CLOSING";
//   else if (item.state()==TState::TIME_WAIT)
//     state_name = "TIME_WAIT";
//   else
//     state_name = "RESET";
//   os << "Current TCP state is: " << state_name << std::endl;
//   return os;
// }

#endif