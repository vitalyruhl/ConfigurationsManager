#pragma once
#include "../ConfigManagerConfig.h"
#if CM_ENABLE_WIFI
#include "ConnectivityCheck.h"
namespace cm { namespace detail {
class ConnectivitySocket final : public ConnectivityTransport {
  int fd_=-1;
  uint32_t target_=0,nonce_=0;
  uint16_t sequence_=0;
public:
  ConnectivitySocket()=default;
  ConnectivitySocket(const ConnectivitySocket&)=delete;
  ConnectivitySocket& operator=(const ConnectivitySocket&)=delete;
  ~ConnectivitySocket() override {cancel();}
  bool start(uint32_t target,uint16_t sequence,uint32_t nonce) override;
  bool poll() override;
  void cancel() override;
};
} }
#endif
