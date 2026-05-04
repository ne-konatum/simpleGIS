# TCP Stream Protocol for MBTiles and DEM Data

## Overview

This document describes the binary protocol for streaming map tiles and elevation data over TCP socket.

## Server Information

- **Default Port**: 5555
- **Protocol**: Binary (big-endian byte order)

## Request Format

### Common Header

All requests start with a 1-byte request type:

| Byte | Description |
|------|-------------|
| 0    | Request Type (1=TILE, 2=ELEVATION, 3=METADATA) |

### TILE_REQUEST (Type = 1)

Request a map tile from MBTiles database.

**Format (10 bytes):**

| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| 0      | 1    | type  | Request type = 1 |
| 1      | 1    | zoom  | Zoom level (0-18) |
| 2-5    | 4    | x     | Tile X coordinate (big-endian quint32) |
| 6-9    | 4    | y     | Tile Y coordinate in XYZ format (big-endian quint32) |

**Example (hex):**
```
01 0C 00 00 01 F4 00 00 01 A2
```
- `01` - TILE_REQUEST
- `0C` - zoom 12
- `00 00 01 F4` - x = 500
- `00 00 01 A2` - y = 418 (XYZ coordinates)

### ELEVATION_REQUEST (Type = 2)

Request elevation data for a specific geographic location.

**Format (17 bytes):**

| Offset | Size | Field     | Description |
|--------|------|-----------|-------------|
| 0      | 1    | type      | Request type = 2 |
| 2-9    | 8    | latitude  | Latitude in degrees (big-endian double) |
| 10-17  | 8    | longitude | Longitude in degrees (big-endian double) |

**Note:** Bytes 1 is unused/padding.

**Example (hex):**
```
02 00 40 5E DD 2F 1A 9F BE 77 40 64 D4 CC 98 00 00 00
```
- `02` - ELEVATION_REQUEST
- `40 5E DD 2F 1A 9F BE 77` - latitude = 125.4567
- `40 64 D4 CC 98 00 00 00` - longitude = 167.8901

### METADATA_REQUEST (Type = 3)

Request metadata about the loaded MBTiles file.

**Format (1 byte):**

| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| 0      | 1    | type  | Request type = 3 |

**Example (hex):**
```
03
```

## Response Format

All responses follow this format:

| Offset | Size | Field   | Description |
|--------|------|---------|-------------|
| 0-3    | 4    | length  | Length of payload data (big-endian quint32) |
| 4      | 1    | status  | 0 = OK, 1 = ERROR |
| 5+     | N    | data    | Payload data (see below) |

### TILE Response (status = 0)

**Data payload:** Raw PNG/JPEG image bytes

### ELEVATION Response (status = 0)

**Data payload (8 bytes):** Height value as big-endian double

If status = 1, no elevation data was found for the requested location.

### METADATA Response (status = 0)

**Data payload:** JSON string containing:
- `minzoom`: Minimum zoom level
- `maxzoom`: Maximum zoom level
- `scheme`: Tile scheme (tms/xyz)
- `name`: Map name
- `description`: Map description
- `version`: MBTiles version

**Example JSON:**
```json
{"minzoom":0,"maxzoom":14,"scheme":"tms","name":"OpenStreetMap","description":"OSM map","version":"1.1"}
```

### Error Response (status = 1)

**Data payload:** UTF-8 encoded error message string

## Usage Examples

### Python Client Example

```python
import socket
import struct

def request_tile(host, port, zoom, x, y):
    # Build request: type(1) + zoom(1) + x(4) + y(4)
    request = struct.pack('>BIBI', 1, zoom, x, y)
    
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.connect((host, port))
        s.sendall(request)
        
        # Read response header (5 bytes)
        header = s.recv(5)
        if len(header) < 5:
            return None
        
        length = struct.unpack('>I', header[0:4])[0]
        status = header[4]
        
        # Read payload
        data = b''
        while len(data) < length:
            chunk = s.recv(length - len(data))
            if not chunk:
                break
            data += chunk
        
        if status == 0:
            return data  # PNG image bytes
        else:
            print(f"Error: {data.decode('utf-8')}")
            return None

def request_elevation(host, port, latitude, longitude):
    # Build request: type(1) + padding(1) + lat(8) + lon(8)
    request = struct.pack('>BBdd', 2, 0, latitude, longitude)
    
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.connect((host, port))
        s.sendall(request)
        
        # Read response header (5 bytes)
        header = s.recv(5)
        if len(header) < 5:
            return None
        
        length = struct.unpack('>I', header[0:4])[0]
        status = header[4]
        
        # Read payload (8 bytes for elevation)
        data = b''
        while len(data) < length:
            chunk = s.recv(length - len(data))
            if not chunk:
                break
            data += chunk
        
        if status == 0 and len(data) == 8:
            height = struct.unpack('>d', data)[0]
            return height
        else:
            return None

# Usage
tile_data = request_tile('localhost', 5555, 12, 500, 418)
if tile_data:
    with open('tile.png', 'wb') as f:
        f.write(tile_data)

elevation = request_elevation('localhost', 5555, 55.7558, 37.6173)
if elevation is not None:
    print(f"Elevation: {elevation} m")
```

### C++ Client Example (Qt)

```cpp
#include <QTcpSocket>
#include <QDataStream>

QByteArray requestTile(const QString &host, quint16 port, 
                       quint8 zoom, quint32 x, quint32 y)
{
    QTcpSocket socket;
    socket.connectToHost(host, port);
    if (!socket.waitForConnected(3000))
        return QByteArray();
    
    // Build request
    QByteArray request;
    QDataStream out(&request, QIODevice::WriteOnly);
    out.setByteOrder(QDataStream::BigEndian);
    out << static_cast<quint8>(1) << zoom << x << y;
    
    socket.write(request);
    socket.waitForReadyRead(3000);
    
    // Read response
    QByteArray response = socket.readAll();
    if (response.size() < 5)
        return QByteArray();
    
    quint32 length = (static_cast<quint8>(response[0]) << 24) |
                     (static_cast<quint8>(response[1]) << 16) |
                     (static_cast<quint8>(response[2]) << 8) |
                     static_cast<quint8>(response[3]);
    quint8 status = static_cast<quint8>(response[4]);
    
    if (status == 0 && response.size() >= 5 + length) {
        return response.mid(5, length);  // PNG data
    }
    
    return QByteArray();
}
```

## Coordinate Systems

### Tile Coordinates

The server uses **XYZ tile coordinates** (same as OpenStreetMap, Google Maps):
- Origin (0,0) is at top-left corner
- X increases eastward
- Y increases southward

**Note:** MBTiles database internally stores tiles in TMS format (Y origin at bottom). The server automatically converts XYZ → TMS when querying the database.

### Geographic Coordinates

- **Latitude**: -90 to +90 degrees (positive = North)
- **Longitude**: -180 to +180 degrees (positive = East)
- Coordinate system: WGS-84 (EPSG:4326)

## Error Codes

| Status | Description |
|--------|-------------|
| 0      | Success |
| 1      | Error (check payload for message) |

Common error messages:
- "No MBTiles file loaded"
- "Tile not found"
- "DEM reader not initialized"
- "Database query failed: ..."

## Notes

1. All multi-byte values use **big-endian** byte order (network byte order)
2. The server supports multiple concurrent clients
3. Connection is closed by client after receiving response
4. Server automatically handles TMS ↔ XYZ conversion for tile coordinates
