# Setup & Deployment Guide

This guide details the environment setup, dependencies, security configuration, and build instructions for the Virtual Industrial IoT Platform.

---

## 1. Prerequisites & Toolchain

* **Operating System**: Windows 10/11 x64
* **Compiler**: Microsoft Visual C++ 2022 (MSVC v143 or newer, C++17 standard)
* **Build System**: CMake 3.20+
* **Package Manager / Libraries**:
  * Eclipse Paho MQTT C++ (`paho-mqttpp3`, typically installed via `vcpkg install paho-mqttpp3:x64-windows`)
  * Windows native WinHTTP (`winhttp.lib`)
* **Supporting Services**:
  * Mosquitto MQTT Broker (port 1883)
  * InfluxDB v2 (port 8086)
  * Grafana (port 3000)

---

## 2. MQTT Broker Setup (Mosquitto)

### Installation
* Download from [mosquitto.org](https://mosquitto.org/download/) or install via Chocolatey:
  ```powershell
  choco install mosquitto
  ```

### Starting the Broker
Start Mosquitto on port 1883 allowing anonymous connections for local development:
```powershell
mosquitto -v -p 1883
```

To test MQTT connectivity:
```powershell
mosquitto_sub -h 127.0.0.1 -p 1883 -t "factory/#" -v
```

---

## 3. InfluxDB v2 Setup

### Launching InfluxDB
* Download InfluxDB v2 from [portal.influxdata.com](https://portal.influxdata.com/downloads/).
* Extract and start the server:
  ```powershell
  .\influxd.exe
  ```
  By default, InfluxDB serves on `http://localhost:8086`.

### Initial Organization & Bucket
1. Open `http://localhost:8086` in your browser.
2. Complete the initial setup:
   * **Organization Name**: `athena`
   * **Initial Bucket Name**: `industrial_iot`
   * **User**: `admin`
3. Generate an API Token:
   * Navigate to **Load Data** $\rightarrow$ **API Tokens**.
   * Create an All-Access or Custom Token for `industrial_iot`.
   * Copy the token value.

### Security & Environment Variables
To ensure security and prevent secret leakage into source control:
* **Never commit tokens or secrets to repository files or Git history.**
* The platform dynamically loads authentication tokens from environment variables, falling back to an empty string if omitted.

Set the token in your active PowerShell session before running:
```powershell
$env:INFLUX_TOKEN = "<your_influxdb_token_here>"
```
Or persist the environment variable for your user account:
```powershell
[System.Environment]::SetEnvironmentVariable('INFLUX_TOKEN', '<your_influxdb_token_here>', 'User')
```

Additional optional overrides:
```powershell
$env:INFLUX_URL = "http://localhost:8086"
$env:INFLUX_ORG = "athena"
$env:INFLUX_BUCKET = "industrial_iot"
```

---

## 4. Grafana Dashboard Setup

### Installation
* Run Grafana standalone or via Docker:
  ```powershell
  docker run -d -p 3000:3000 --name grafana grafana/grafana
  ```
* Open `http://localhost:3000` (default login: `admin` / `admin`).

### Adding InfluxDB Data Source
1. In Grafana, navigate to **Connections** $\rightarrow$ **Data Sources** $\rightarrow$ **Add data source**.
2. Select **InfluxDB**.
3. Set Query Language to **Flux**.
4. Configure connection parameters:
   * **URL**: `http://localhost:8086` (or `http://host.docker.internal:8086` if running Grafana in Docker)
   * Disable **Basic Auth**.
   * Under **InfluxDB Details**:
     * **Organization**: `athena`
     * **Token**: `<your_influxdb_token>`
     * **Default Bucket**: `industrial_iot`
5. Click **Save & test** to verify green checkmark connectivity.

### Importing the Dashboard
1. Go to **Dashboards** $\rightarrow$ **New** $\rightarrow$ **Import**.
2. Click **Upload dashboard JSON file** and select:
   `dashboards/industrial_factory_dashboard.json`
3. Select your InfluxDB data source (mapped automatically via `${datasource}`) and click **Import**.

---

## 5. Building and Running

### 1. Build Binaries
```powershell
# Configure CMake with vcpkg integration
cmake -B build

# Build Release binaries for both platform and tests
cmake --build build --config Release
```

### 2. Run Automated Verification Tests
```powershell
.\build\Release\industrial-iot-tests.exe
```

### 3. Run Simulation Executable
```powershell
$env:INFLUX_TOKEN = "<your_token_here>"
.\build\Release\virtual-industrial-iot.exe
```
