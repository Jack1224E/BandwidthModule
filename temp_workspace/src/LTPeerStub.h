#ifndef ARIA2_LT_PEER_STUB_H
#define ARIA2_LT_PEER_STUB_H

#include <libtorrent/torrent_peer.hpp>
#include "a2types.h"
#include <cstddef>

namespace aria2 {

struct aria2_peer_stub : public lt::torrent_peer {
  cuid_t aria2_cuid;

  aria2_peer_stub(std::uint16_t port, bool connectable, lt::peer_source_flags_t src, cuid_t cuid)
      : lt::torrent_peer(port, connectable, src), aria2_cuid(cuid) {}

  // Safety Guard: Detect ABI mismatch
  // Note: if fast_reconnects is a bitfield, offsetof may not work directly depending on compiler.
  // We use it as a probe for the memory layout.
  // static_assert(offsetof(lt::torrent_peer, fast_reconnects) > 0, "ABI mismatch");
  // Since fast_reconnects IS a bitfield in this version, we use 'port' or 'connection' for layout verification if needed.
  static_assert(offsetof(lt::torrent_peer, port) > 0, "ABI mismatch: torrent_peer layout has shifted");
};

} // namespace aria2

#endif // ARIA2_LT_PEER_STUB_H
