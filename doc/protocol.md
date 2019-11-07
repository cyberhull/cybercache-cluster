
CyberCache Cluster: Binary Client/Server Protocol Format
========================================================

The protocol is binary and can transfer integers, strings, and lists of strings,
as well as chunks of binary data. If it is necessary to pass anything other than
an integer number (up to 32 bits, signed or unsigned) or a string, it is passed
as a string and client or server (whoever is the recepient) then parses it. See 
notes below on 64-bit extensions.

> **NOTE**: this document describes low-level protocol details -- building
> blocks from which high-level commands are assembled. Command IDs, as well as
> information on which building blocks comprise which command, can be found in
> the Command Interface reference manual; please see `cybercache(1)`.

The protocol is designed to cover all Magento 2 session store and cache back-end
needs in the most efficient way; specifically, so that:

- server could easily separate auxiliary data (only used by worker threads
  during processing of the request as well as replication/persisting) and
  permanent data that are to be stored in the cache,

- server and clients would always receive data of known size, and said size 
  should be computable upon reception of the first fixed, small known number of
  bytes,

- server and clients would exchange minimum amounts of data (hence numbers, 
  counts, and lengths are always encoded to only take minimum required space).

The protocol does not support any kind of "begin transaction"/"end transaction"
commands -- but that is because its individual commands are designed in such a
way that, what would be accomplished using a transaction in, say, Redis working
as Magento session store, is accomplished with a single command of CyberCache
protocol. In other words, its individual commands **are** transactions,
automatically taking care of cache data integrity. It is essentially a high-
level protocol specifically tailored for Magento 2 needs, but it can also be
utilized by any PHP session store, or by frameworks using Zend cache API.

Requests to Server
------------------

This section describes data sent to the server.

### First Chunk: Command Descriptor (`DESCRIPTOR`) ###

Single byte, always present:

- bits 0 and 1: authentication type
    - 00: no authentication
    - 01: user-level authentication
    - 10: admin-level authentication
    - 11: bulk authentication (e.g. during replication or loading from binlog)

- bits 2 and 3: header size
    - 00: no header size (only ID and password can be present)
    - 01: header size is one byte
    - 10: header size is a 16-bit word
    - 11: header size is a 32-bit word
- bits 4 and 5: payload chunk size
    - 00: no payload chunk
    - 01: [un]compressed payload chunk, sizes are bytes
    - 10: [un]compressed payload chunk, sizes are 16-bit words
    - 11: [un]compressed payload chunk, sizes are 32-bit words
- bit 6: compressed (1) or uncompressed (0) payload
- bit 7: marker byte `0xC3` present (1) or absent (0) at the very end of the command

### Second Chunk: Command Header (`HEADER`) ###

Header data is only kept until the command is fully processed (executed and 
optionally replicated and/or persisted); it is referred to in the external
documentation as

    `HEADER { <ID> [PASSWORD] [PAYLOAD_INFO] <chunk> [ <chunk> [...]] }`

> **IMPORTANT**: if a command does not have either a sequence of data chunks
> (see below) or payload (i.e. it only consists of mandatory command ID and
> optional password hash code) then header size is *not* stored in the header,
> and full command length is then determined by descriptor analysis. Even though
> it would also be possible to fully determine header length if payload
> compression information was stored without header size byte(s) in case of
> absense of the sequence of data chunks, it's been desided not to do that: the
> payload is likely to be big, so trading a bit of extra convenience for one
> byte saving did not look worthwhile.

Header consists of the following:

- header size (present if header size in Command Descriptor is not `00`):
    - byte if header size bits in Command Descriptor are `01`
    - 16-bit word if header size bits in Command Descriptor are `10`
    - 32-bit word if header size bits in Command Descriptor are `11`

- command ID:
    - single byte, always present

- password hash code (present if authentication type bits in Command Descriptor are not `00`):
    - eigth bytes: password hash code XOR-ed with a constant

- compressor type byte (present if "compressed payload" bit in descriptor is `1`):
    - 0x00: none
    - 0x01: lzf
    - 0x02: snappy
    - 0x03: lz4
    - 0x04: lzss3
    - 0x05: brotli
    - 0x06: zstd
    - 0x07: zlib
    - 0x08: lzham

- payload size (present if payload chunk size is not `0x00`):
    - byte if payload chunk size bits in Command Descriptor are `01`
    - 16-bit word if payload chunk size bits in Command Descriptor are `10`
    - 32-bit word if payload chunk size bits in Command Descriptor are `11`

- uncompressed payload size (present if "compressed payload" bit in descriptor is `1`):
    - byte if payload chunk size bits in Command Descriptor are `01`
    - 16-bit word if payload chunk size bits in Command Descriptor are `10`
    - 32-bit word if payload chunk size bits in Command Descriptor are `11`

- sequence of data chunks; each starts with data type descriptor byte, high bits define chunk type:
    - 00: chunk is an integer number, value bias is lower 6 bits (value is `8..71`)
    - 01: chunk is a string, length bias is lower 6 bits (length is `8..71`):
        - next 8..71 bytes: string characters
    - 10: chunk is a list of strings, element count bias in lower 6 bits (element count `8..71`); strings
      are in VLQ format:
        - *byte*: element length; if it is `255`, then length is `255+<next-byte>` IF `<next-byte>` is less
          than 255 (retrieval of bytes continues until byte less than 255 is found); number of bytes needed
          for encoding is `<string-length>/255+1`; this format had been chosen over "classic" VLQ because,
          given the protocol's intended use, it is much more likely to see a string of length `128..254`
          than of length greater than `254`
        - *next `<length>` bytes*: element data (string characters, terminating `0` is *not* stored)
    - 11: chunk is a so-called *special value*, type is defined by next 3 bits:
        - 000: negative integer value `-1..-8`; that is, `minus (lower bits plus one)`
        - 001: negative integer value `-9...`, number of extra bytes specified by lower 3 bits plus 1
        - 010: small integer value `0..7`, value is lower 3 bits
        - 011: short string of length `0..7`, length is lower 3 bits
        - 100: short list with `0..7` elements, count is lower 3 bits; strings in VLQ format
        - 101: big positive integer value `72...`, number of extra bytes specified by lower 3 bits plus 1
        - 110: long string of length `72...`, number of extra bytes specified by lower 3 bits plus 1
        - 111: long list with `72...` elements, number of extra bytes specified by lower 3 bits plus 1,
          strings are in VLQ format

Even though big integers (both positive and negative) and long strings and lists
have extra data of length that is encoded with three bits, only two of those
bits are currently used. As a result, signed and unsigned integers larger than
32-bit *would* have to be encoded as strings if they were ever needed (the
high-level server protocol does *not* currently require transfer of any 64-bit
integers as separate entities). This restriction may be lifted in the future if
the need to send/receive bigger-than-32 integers arises.

If header is followed by bytes of extra data, the data is referred to in the 
external documentation as `CHUNK(<type>)`, where `type` is one of the following:

- `NUMBER`: chunk descriptor, optional 1..4 extra bytes specifying bias of a positive or negative integer,
- `STRING`: optional 1..4 bytes specifying string length bias, followed by string characters,
- `LIST`: optional 1..4 bytes specifying list count bias, followed by list elements,

### Third Chunk: Payload (`PAYLOAD`) ###

Present if payload chunk size bits in Command Descriptor are not `00`:

- compressed data (`<payload-size>` bytes)

### Fourth Chunk: Integrity Check (`MARKER`) ###

Present if "marker byte present" bit in Command Descriptor is `1`: byte `0xC3`

Server Responses
----------------

Throughout external documents, the four types of responses (respective
descriptors) are denoted as `OK`, `DATA` (header in standard format, same for
both commands and responses, payload is binary data), `LIST` (header in standard
format; payload is a list of strings), and `ERROR` (the string in the header is
an error message). Consequently, various combinations of the Response Descriptor
and optional data are denoted as follows:

- `OK [ MARKER ]`: success, no data (descriptor only),

- `ERROR HEADER { CHUNK(STRING) } [ MARKER ]`: failure, error message is returned,

- `DATA [ HEADER { [ PAYLOAD_INFO ] CHUNK(...) ... } ] [ PAYLOAD ] [ MARKER ]`:
  success; optional structured data in the header is returned; if there is
  payload, it contains session of FPC entry data,

- `LIST [ HEADER { [ PAYLOAD_INFO ] CHUNK(...) ... } ] [ PAYLOAD ] [ MARKER ]`:
  success, optional structured data in the header is returned; if there is
  payload, it contains a list (number of entries is in the header).

### First Chunk: Response Descriptor ###

> **IMPORTANT**: just as with command header (see above), if response does not 
> have either a sequence of data chunks or a payload, header size is *not* 
> stored, and response header then consists of a single descriptor byte.

Single byte, always present:

- bits 0 and 1: response type:
    - 00: success, no extra data
    - 01: success, payload (if present) is binary data
    - 10: success, payload (if present) is list of strings
    - 11: error, header contains error message
- bits 2 and 3: header size
    - 00: no header size (must be an `OK` response)
    - 01: header size is one byte
    - 10: header size is a 16-bit word
    - 11: header size is a 32-bit word
- bits 4 and 5: payload chunk size
    - 00: no payload chunk
    - 01: [un]compressed payload chunk, sizes are bytes
    - 10: [un]compressed payload chunk, sizes are 16-bit words
    - 11: [un]compressed payload chunk, sizes are 32-bit words
- bit 6: compressed (1) or uncompressed (0) payload
- bit 7: marker byte `0xC3` present (1) or absent (0) at the very end of the response

### Second Chunk: Response Header ###

Consists of the following:

- header size (present if header size in Response Descriptor is not `00`):
    - byte if header size bits in Response Descriptor are `01`
    - 16-bit word if header size bits in Response Descriptor are `10`
    - 32-bit word if header size bits in Response Descriptor are `11`

- compressor type byte (present if "compressed payload" bit in descriptor is `1`):
    - 0x00: none
    - 0x01: lzf
    - 0x02: snappy
    - 0x03: lz4
    - 0x04: lzss3
    - 0x05: brotli
    - 0x06: zstd
    - 0x07: zlib
    - 0x08: lzham

- payload size (present if payload chunk size is not `0x00`):
    - byte if payload chunk size bits in Response Descriptor are `01`
    - 16-bit word if payload chunk size bits in Response Descriptor are `10`
    - 32-bit word if payload chunk size bits in Response Descriptor are `11`

- uncompressed payload size (present if "compressed payload" bit in descriptor is `1`):
    - byte if payload chunk size bits in Response Descriptor are `01`
    - 16-bit word if payload chunk size bits in Response Descriptor are `10`
    - 32-bit word if payload chunk size bits in Response Descriptor are `11`

- sequence of data chunks; format of the chunks is the same as that used in the 
  Command Header (see above).

### Third Chunk: Payload (`PAYLOAD`) ###

Present if payload chunk size bits in Command Descriptor are not `00`:

- compressed data (`<payload-size>` bytes)

### Fourth Chunk: Integrity Check (`MARKER`) ###

Present if "marker byte present" bit in Response Descriptor is `1`: byte `0xC3`
