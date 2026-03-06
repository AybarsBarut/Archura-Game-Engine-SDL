# Archura Dedicated Server Guide

## Genel Bakış

Archura Engine artık iki ayrı executable ile geliyor:
- **ArchuraEngine.exe** - Grafik arayüzlü oyun client'ı
- **ArchuraServer.exe** - Headless dedicated server (128 tickrate)

## Sistem Gereksinimleri

### Minimum
- **CPU**: 2 core, 2.0 GHz
- **RAM**: 512 MB
- **Network**: 1 Mbps upload (16 oyuncu için)
- **OS**: Windows 10/11 64-bit

### Önerilen (128 Tickrate)
- **CPU**: 4 core, 3.0 GHz+
- **RAM**: 1 GB
- **Network**: 5 Mbps upload
- **OS**: Windows 10/11 64-bit

## Hızlı Başlangıç

### 1. Server'ı Başlatma

En basit yöntem:
```batch
start_server.bat
```

Bu script `server_config.json` dosyasını okur ve server'ı başlatır.

### 2. Manuel Başlatma

```batch
ArchuraServer.exe --config server_config.json
```

### 3. Komut Satırı Parametreleri

```batch
ArchuraServer.exe --port 27015 --tickrate 128 --maxplayers 16 --map dm_arena --verbose
```

## Konfigürasyon

### server_config.json

```json
{
  "server_name": "My Archura Server",
  "port": 27015,
  "max_players": 16,
  "tickrate": 128,
  "snapshot_rate": 64,
  "map": "dm_arena",
  "game_mode": "deathmatch",
  "password": "",
  "client_timeout": 30.0,
  "connection_timeout": 10.0,
  "rcon_password": "",
  "enable_rcon": false,
  "verbose_logging": false,
  "log_file": "server.log"
}
```

### Parametreler

| Parametre | Açıklama | Default |
|-----------|----------|---------|
| `server_name` | Server adı (server browser'da görünür) | "Archura Server" |
| `port` | Network port | 27015 |
| `max_players` | Maksimum oyuncu sayısı | 16 |
| `tickrate` | Server güncelleme hızı (Hz) | 128 |
| `snapshot_rate` | Client'a gönderilen snapshot hızı (Hz) | 64 |
| `map` | Başlangıç haritası | "dm_arena" |
| `game_mode` | Oyun modu | "deathmatch" |
| `password` | Server şifresi (boş = şifresiz) | "" |
| `client_timeout` | Client timeout süresi (saniye) | 30.0 |
| `connection_timeout` | Bağlantı timeout süresi (saniye) | 10.0 |
| `verbose_logging` | Detaylı log | false |

## Komut Satırı Seçenekleri

```
ArchuraServer [options]

Options:
  --config <file>      JSON config dosyasından yükle
  --port <port>        Server port (default: 27015)
  --tickrate <rate>    Server tickrate (default: 128)
  --maxplayers <num>   Maksimum oyuncu (default: 16)
  --map <name>         Harita adı (default: dm_arena)
  --name <name>        Server adı
  --password <pass>    Server şifresi
  --verbose            Detaylı logging aktif et
  --help, -h           Bu yardım mesajını göster
```

## Port Forwarding

Server'ınızı internete açmak için router'ınızda port forwarding yapmanız gerekir:

1. Router admin paneline giriş yapın (genellikle 192.168.1.1)
2. Port Forwarding / Virtual Server bölümüne gidin
3. Yeni kural ekleyin:
   - **Protocol**: TCP + UDP
   - **External Port**: 27015 (veya seçtiğiniz port)
   - **Internal Port**: 27015
   - **Internal IP**: Server bilgisayarınızın local IP'si
4. Kaydedin ve router'ı yeniden başlatın

### IP Adresinizi Öğrenme

Dış IP adresinizi öğrenmek için: https://whatismyipaddress.com/

Oyuncular server'ınıza bağlanmak için bu IP'yi kullanacak:
```
connect <your_ip>:27015
```

## Performans Optimizasyonu

### Tickrate Ayarlama

128 tickrate CPU yoğun olabilir. Eğer performans sorunu yaşıyorsanız:

**64 Tickrate** (Dengeli):
```json
{
  "tickrate": 64,
  "snapshot_rate": 32
}
```

**32 Tickrate** (Düşük performans):
```json
{
  "tickrate": 32,
  "snapshot_rate": 20
}
```

### Oyuncu Sayısı

Daha fazla oyuncu = daha fazla CPU ve bandwidth:
- **8 oyuncu**: ~2 Mbps upload
- **16 oyuncu**: ~5 Mbps upload
- **32 oyuncu**: ~10 Mbps upload

## Sorun Giderme

### Server Başlamıyor

1. **Port zaten kullanımda**:
   ```
   Error: Bind failed
   ```
   Çözüm: Farklı bir port kullanın (`--port 27016`)

2. **Config dosyası bulunamadı**:
   ```
   Warning: server_config.json not found
   ```
   Çözüm: Config dosyasını oluşturun veya komut satırı parametreleri kullanın

### Düşük Tickrate

Eğer verbose mode'da tickrate 128'den düşükse:
```
Tickrate: 95 ticks/sec  # Düşük!
```

Çözümler:
- Tickrate'i düşürün (64 veya 32)
- Daha güçlü CPU kullanın
- Arka planda çalışan programları kapatın

### Client Bağlanamıyor

1. **Firewall**: Windows Defender Firewall'da ArchuraServer.exe'ye izin verin
2. **Port Forwarding**: Router ayarlarını kontrol edin
3. **IP Adresi**: Doğru dış IP adresini kullandığınızdan emin olun

## Gelişmiş Kullanım

### Birden Fazla Server

Farklı portlarda birden fazla server çalıştırabilirsiniz:

**Server 1** (Port 27015):
```batch
ArchuraServer.exe --port 27015 --map dm_arena
```

**Server 2** (Port 27016):
```batch
ArchuraServer.exe --port 27016 --map dm_dust
```

### Otomatik Yeniden Başlatma

Server crash olursa otomatik yeniden başlatmak için:

```batch
@echo off
:restart
echo Starting server...
ArchuraServer.exe --config server_config.json
echo Server stopped. Restarting in 5 seconds...
timeout /t 5
goto restart
```

## Loglar

Server logları `server.log` dosyasına yazılır (verbose mode aktifse).

Log seviyelerini kontrol etmek için:
```json
{
  "verbose_logging": true,
  "log_file": "server.log"
}
```

## Destek

Sorunlarınız için:
- GitHub Issues: [https://github.com/AybarsBarut/Archura-Game-Engine-SDL]
- Email: support@archura.com

## Sürüm Notları

### v1.0.0 (Current)
-  128 tickrate desteği
-  Headless dedicated server
-  JSON config dosyası
-  Komut satırı parametreleri
-  Verbose logging
-  UDP network protocol (yakında)
-  Client prediction (yakında)
-  Server browser (yakında)
