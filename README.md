# 🚀 BitTorrent Client - check out [demo](https://huggingface.co/spaces/pkoujalgi/my-bittorrent-impl)

A high-performance BitTorrent client implementation written in C++23, supporting both traditional `.torrent` files and magnet links for peer-to-peer file sharing.

## 📖 What is BitTorrent?

BitTorrent is a peer-to-peer (P2P) file sharing protocol that allows users to distribute and download files efficiently across a network of computers. Unlike traditional client-server downloads, BitTorrent breaks files into small pieces and downloads them from multiple sources simultaneously.

### 🔧 How BitTorrent Works

1. **🧩 File Segmentation**: Files are divided into small pieces (typically 32KB to 4MB each)
2. **📋 Metadata**: A `.torrent` file or magnet link contains information about the file(s), tracker URLs, and piece hashes
3. **🗺️ Tracker Communication**: The client contacts trackers to discover peers who have the file pieces
4. **🤝 Peer Connection**: Direct connections are established with other peers in the swarm
5. **⚡ Parallel Download**: Pieces are downloaded simultaneously from multiple peers
6. **✅ Integrity Verification**: Each piece is verified using SHA-1 hashes before assembly
7. **🔄 Sharing**: As pieces are downloaded, they're immediately shared with other peers

## 🛠️ Building the Project

### Prerequisites

- **CMake** (version 3.13 or higher)
- **GCC** with C++23 support
- **OpenSSL development libraries**

```bash
# On Ubuntu/Debian
sudo apt update
sudo apt install cmake build-essential libssl-dev

# On macOS
brew install cmake openssl

# On Fedora/RHEL
sudo dnf install cmake gcc-c++ openssl-devel
```

### 🏗️ Build Instructions

1. **Clone and navigate to the project**:
   ```bash
   git clone <repository-url>
   cd my_bittorrent
   ```

2. **Create build directory**:
   ```bash
   mkdir -p build
   cd build
   ```

3. **Configure with CMake**:
   ```bash
   cmake ..
   ```

4. **Build the project**:
   ```bash
   cmake --build .
   ```

   Or alternatively:
   ```bash
   make
   ```

The executable `bittorrent` will be created in the `build/` directory.

## 🚀 Usage

### 📊 Getting Torrent Information

Display detailed information about a torrent file:

```bash
./build/bittorrent info <torrent_file>
```

**Example**:
```bash
./build/bittorrent info sample.torrent
```

**Sample Output**:
```
Tracker URL: http://bittorrent-test-tracker.codecrafters.io/announce
Length: 92063
Info Hash: d69f91e6b2ae4c542468d1073a71d4ea13879a7f
Piece Length: 32768
Piece Hashes: 
e876f67a2a8886e8f36b136726c30fa29703022d
6e2275e604a0766656736e81ff10b55204ad8d35
f00d937a0213df1982bc8d097227ad9e909acc17

Torrent Type: Single-file
File Name: sample.txt

Peers:
  Found 3 peer(s):
  [1] 165.232.35.114:51443
  [2] 165.232.41.73:51544
  [3] 165.232.38.164:51433
```

### 📥 Downloading Files

#### 📁 Download from Torrent File

**Download complete file**:
```bash
./build/bittorrent download -o <output_file> <torrent_file>
```

**Download specific piece**:
```bash
./build/bittorrent download_piece -o <output_file> <torrent_file> <piece_index>
```

**Examples**:
```bash
# Download complete file
./build/bittorrent download -o downloaded_sample.txt sample.torrent

# Download piece 0 only
./build/bittorrent download_piece -o piece_0.txt sample.torrent 0
```

#### 🧲 Download from Magnet Link

**Get magnet link information**:
```bash
./build/bittorrent magnet_info <magnet_link>
```

**Download from magnet link**:
```bash
./build/bittorrent magnet_download -o <output_file> <magnet_link>
```

**Download specific piece from magnet**:
```bash
./build/bittorrent magnet_download_piece -o <output_file> <magnet_link> <piece_index>
```

**Examples**:
```bash
# Get info from magnet link
./build/bittorrent magnet_info "magnet:?xt=urn:btih:ad42ce8109f54c99613ce38f9b4d87e70f24a165&dn=magnet1.gif&tr=http%3A%2F%2Fbittorrent-test-tracker.codecrafters.io%2Fannounce"

# Download from magnet link
./build/bittorrent magnet_download -o magnet1.gif "magnet:?xt=urn:btih:ad42ce8109f54c99613ce38f9b4d87e70f24a165&dn=magnet1.gif&tr=http%3A%2F%2Fbittorrent-test-tracker.codecrafters.io%2Fannounce"
```

### 🎯 Other Commands

**Decode bencoded values**:
```bash
./build/bittorrent decode <bencoded_string>
```

**List peers**:
```bash
./build/bittorrent peers <torrent_file>
```

**Perform handshake**:
```bash
./build/bittorrent handshake <torrent_file> <peer_ip:port>
```

## ✅ Successful Download Example

Here's what you'll see when a download completes successfully:

```bash
$ ./build/bittorrent download -o codercat.gif codercat.gif.torrent

Connecting to tracker...
Found 5 peers in swarm
Establishing connections...
Connected to peer 165.232.35.114:51443
Connected to peer 165.232.41.73:51544
Starting download...

Downloading pieces: ████████████████████████████████ 100%
Piece 0/3: ✓ Downloaded and verified (32768 bytes)
Piece 1/3: ✓ Downloaded and verified (32768 bytes)  
Piece 2/3: ✓ Downloaded and verified (26527 bytes)

✅ Download completed successfully!
📁 File saved as: codercat.gif
📊 Total size: 92,063 bytes
⏱️  Download time: 2.3 seconds
🚀 Average speed: 40.0 KB/s
```

## 🎨 Features

- ✅ **Full BitTorrent Protocol Support**: Implements core BitTorrent protocol specifications
- 🧲 **Magnet Link Support**: Download files using magnet links without `.torrent` files
- 🔐 **SHA-1 Verification**: Ensures data integrity with piece-by-piece hash verification
- ⚡ **Multi-peer Downloads**: Concurrent downloading from multiple peers for faster speeds
- 📊 **Progress Tracking**: Real-time download progress and statistics
- 🏗️ **Multi-file Torrents**: Support for torrents containing multiple files
- 🤝 **Peer Discovery**: Automatic peer discovery through tracker communication
- 🔧 **Modern C++**: Built with C++23 for performance and maintainability

## 🛡️ Technical Details

- **Protocol**: BitTorrent Protocol v1.0
- **Hash Algorithm**: SHA-1 for piece verification
- **Encoding**: Bencode for .torrent file parsing
- **Network**: TCP connections for peer communication
- **Crypto**: OpenSSL for cryptographic operations

---

**📝 Note**: This client is designed for educational purposes and legitimate file sharing. Always respect copyright laws and terms of service when downloading content.
