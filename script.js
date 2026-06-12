// ═══════════════════════════════════════════════════════════
// 🚦 SMART TRAFFIC MANAGEMENT SYSTEM - JAVASCRIPT
// ═══════════════════════════════════════════════════════════

let vehicleTotal = 0;
let esp32IP = "192.168.1.100";  // Default IP, user can change this

// Load saved IP from localStorage
if(localStorage.getItem("esp32IP")) {
  esp32IP = localStorage.getItem("esp32IP");
  document.getElementById("esp32IP").value = esp32IP;
}

// ═══════════════════════════════════════════════════════════
// ⚙️ CONFIGURATION PANEL TOGGLE
// ═══════════════════════════════════════════════════════════
function toggleConfig() {
  const configContent = document.getElementById("configContent");
  const configPanel = document.getElementById("configPanel");
  const toggleBtn = document.getElementById("configToggleBtn");
  
  if (configContent.classList.contains("hidden")) {
    // Expand
    configContent.classList.remove("hidden");
    configPanel.classList.remove("minimized");
    toggleBtn.textContent = "−";
    addLog("⚙️ Configuration panel expanded");
  } else {
    // Minimize
    configContent.classList.add("hidden");
    configPanel.classList.add("minimized");
    toggleBtn.textContent = "+";
    addLog("⚙️ Configuration panel minimized");
  }
}

// ═══════════════════════════════════════════════════════════
// 📊 INITIALIZE CHARTS
// ═══════════════════════════════════════════════════════════
const vehicleCtx = document.getElementById('vehicleChart');
const vehicleChart = new Chart(vehicleCtx, {
  type: 'bar',
  data: {
    labels: ['Lane 1', 'Lane 2', 'Lane 3', 'Lane 4'],
    datasets: [{
      label: 'Vehicle Count',
      data: [0, 0, 0, 0],
      backgroundColor: ['#FF6384', '#36A2EB', '#FFCE56', '#4BC0C0'],
      borderColor: ['#FF6384', '#36A2EB', '#FFCE56', '#4BC0C0'],
      borderWidth: 2
    }]
  },
  options: {
    responsive: true,
    maintainAspectRatio: true,
    plugins: {
      legend: {
        labels: {
          color: '#ffffff'
        }
      }
    },
    scales: {
      y: {
        beginAtZero: true,
        ticks: {
          color: '#ffffff'
        },
        grid: {
          color: 'rgba(255, 255, 255, 0.1)'
        }
      },
      x: {
        ticks: {
          color: '#ffffff'
        },
        grid: {
          color: 'rgba(255, 255, 255, 0.1)'
        }
      }
    }
  }
});

const densityCtx = document.getElementById('densityChart');
const densityChart = new Chart(densityCtx, {
  type: 'line',
  data: {
    labels: ['Lane 1', 'Lane 2', 'Lane 3', 'Lane 4'],
    datasets: [{
      label: 'Traffic Density',
      data: [0, 0, 0, 0],
      borderColor: '#36A2EB',
      backgroundColor: 'rgba(54, 162, 235, 0.2)',
      tension: 0.4,
      fill: true,
      borderWidth: 3
    }]
  },
  options: {
    responsive: true,
    maintainAspectRatio: true,
    plugins: {
      legend: {
        labels: {
          color: '#ffffff'
        }
      }
    },
    scales: {
      y: {
        beginAtZero: true,
        ticks: {
          color: '#ffffff'
        },
        grid: {
          color: 'rgba(255, 255, 255, 0.1)'
        }
      },
      x: {
        ticks: {
          color: '#ffffff'
        },
        grid: {
          color: 'rgba(255, 255, 255, 0.1)'
        }
      }
    }
  }
});

// ═══════════════════════════════════════════════════════════
// 🎨 UI UPDATE FUNCTIONS
// ═══════════════════════════════════════════════════════════
function resetLights() {
  for(let i = 1; i <= 4; i++) {
    document.getElementById("r" + i).classList.remove("active");
    document.getElementById("y" + i).classList.remove("active");
    document.getElementById("g" + i).classList.remove("active");
  }
}

function resetEmergencyIndicators() {
  for(let i = 1; i <= 4; i++) {
    document.getElementById("emg" + i).classList.remove("active");
    document.getElementById("emergencyBadge" + i).innerHTML = "";
  }
}

function setLaneGreen(lane) {
  resetLights();
  document.getElementById("g" + lane).classList.add("active");
  document.getElementById("currentLane").innerText = "Lane " + lane;
}

function showEmergencyOnLane(lane) {
  resetEmergencyIndicators();
  document.getElementById("emg" + lane).classList.add("active");
  document.getElementById("emergencyBadge" + lane).innerHTML = 
    '<div class="emergency-badge">🚑 AMBULANCE DETECTED</div>';
}

function updateDensityBadges(vehicles) {
  for(let i = 0; i < 4; i++) {
    let badge = document.getElementById("densityBadge" + (i + 1));
    let density = vehicles[i];
    
    if(density < 20) {
      badge.innerHTML = '<span class="density-low">🟢 LOW</span>';
    } else if(density < 50) {
      badge.innerHTML = '<span class="density-medium">🟡 MEDIUM</span>';
    } else {
      badge.innerHTML = '<span class="density-high">🔴 HIGH</span>';
    }
  }
}

function updateTimers(activeLane, timer) {
  for(let i = 1; i <= 4; i++) {
    let timerEl = document.getElementById("timer" + i);
    if(i === activeLane) {
      timerEl.innerText = "⏱️ " + timer + "s";
      timerEl.classList.remove("inactive");
    } else {
      timerEl.innerText = "⏱️ --";
      timerEl.classList.add("inactive");
    }
  }
}

function addLog(text) {
  const logs = document.getElementById("logs");
  const item = document.createElement("div");
  item.className = "logItem";
  item.innerText = new Date().toLocaleTimeString() + " - " + text;
  logs.prepend(item);
  
  // Keep only last 50 logs
  while(logs.children.length > 50) {
    logs.removeChild(logs.lastChild);
  }
}

// ═══════════════════════════════════════════════════════════
// 🎛️ CONTROL FUNCTIONS
// ═══════════════════════════════════════════════════════════
function manualLane(lane) {
  fetch("http://" + esp32IP + "/manualLane?lane=" + lane)
    .then(res => res.text())
    .then(data => {
      addLog("🎛️ Manual control: Lane " + lane + " activated");
      setLaneGreen(lane);
      document.getElementById("controlMode").innerText = "MANUAL";
      document.getElementById("controlMode").style.color = "#FFC107";
    })
    .catch(error => {
      console.error("Error:", error);
      addLog("❌ Failed to set manual lane " + lane);
    });
}

function autoMode() {
  fetch("http://" + esp32IP + "/autoMode")
    .then(res => res.text())
    .then(data => {
      addLog("🤖 Automatic mode enabled");
      document.getElementById("controlMode").innerText = "AUTO";
      document.getElementById("controlMode").style.color = "#4CAF50";
    })
    .catch(error => {
      console.error("Error:", error);
      addLog("❌ Failed to enable auto mode");
    });
}

function allRed() {
  fetch("http://" + esp32IP + "/allRed")
    .then(res => res.text())
    .then(data => {
      resetLights();
      for(let i = 1; i <= 4; i++) {
        document.getElementById("r" + i).classList.add("active");
      }
      document.getElementById("currentLane").innerText = "All Red";
      document.getElementById("controlMode").innerText = "MANUAL";
      document.getElementById("controlMode").style.color = "#FFC107";
      addLog("🛑 All lanes set to RED");
    })
    .catch(error => {
      console.error("Error:", error);
      addLog("❌ Failed to set all red");
    });
}

function manualEmergency(lane) {
  fetch("http://" + esp32IP + "/manualEmergency?lane=" + lane)
    .then(res => res.text())
    .then(data => {
      setLaneGreen(lane);
      showEmergencyOnLane(lane);
      document.getElementById("emergencyStatus").innerText = "YES - Lane " + lane;
      document.getElementById("emergencyStatus").style.color = "red";
      addLog("🚑 Manual emergency activated for Lane " + lane);
    })
    .catch(error => {
      console.error("Error:", error);
      addLog("❌ Failed to activate emergency for lane " + lane);
    });
}

function streetON() {
  fetch("http://" + esp32IP + "/streetON")
    .then(res => res.text())
    .then(data => {
      document.getElementById("streetLight").innerText = "ON";
      document.getElementById("streetLight").style.color = "yellow";
      document.getElementById("streetMode").innerText = "MANUAL MODE";
      addLog("💡 Street Lights turned ON (Manual)");
    })
    .catch(error => {
      console.error("Error:", error);
      addLog("❌ Failed to turn street lights ON");
    });
}

function streetOFF() {
  fetch("http://" + esp32IP + "/streetOFF")
    .then(res => res.text())
    .then(data => {
      document.getElementById("streetLight").innerText = "OFF";
      document.getElementById("streetLight").style.color = "white";
      document.getElementById("streetMode").innerText = "MANUAL MODE";
      addLog("💡 Street Lights turned OFF (Manual)");
    })
    .catch(error => {
      console.error("Error:", error);
      addLog("❌ Failed to turn street lights OFF");
    });
}

function streetAuto() {
  fetch("http://" + esp32IP + "/streetAuto")
    .then(res => res.text())
    .then(data => {
      document.getElementById("streetMode").innerText = "AUTO MODE";
      addLog("💡 Street Lights set to AUTO mode");
    })
    .catch(error => {
      console.error("Error:", error);
      addLog("❌ Failed to set street lights to AUTO");
    });
}

function emergencyOff() {
  fetch("http://" + esp32IP + "/emergencyOff")
    .then(res => res.text())
    .then(data => {
      resetEmergencyIndicators();
      document.getElementById("emergencyStatus").innerText = "NO";
      document.getElementById("emergencyStatus").style.color = "white";
      addLog("🚑 Emergency mode manually cleared");
    })
    .catch(error => {
      console.error("Error:", error);
      addLog("❌ Failed to clear emergency mode");
    });
}

// ═══════════════════════════════════════════════════════════
// 📡 FETCH DATA FROM ESP32
// ═══════════════════════════════════════════════════════════
function fetchRealData() {
  fetch("http://" + esp32IP + "/data")
    .then(res => {
      if(!res.ok) throw new Error("Network response was not ok");
      return res.json();
    })
    .then(data => {
      // Debug: Log received data to console
      console.log("📡 Data received:", data);
      
      // Update connection status
      document.getElementById("connectionStatus").innerText = "ONLINE";
      document.getElementById("connectionStatus").style.color = "#4CAF50";
      
      // Update active lane
      if(data.lane) {
        setLaneGreen(data.lane);
      }

      // Update timer
      if(data.timer !== undefined) {
        updateTimers(data.lane, data.timer);
      }

      // Handle emergency
      if(data.emergency && data.emergency !== 0) {
        showEmergencyOnLane(data.emergency);
        document.getElementById("emergencyStatus").innerText = "YES - Lane " + data.emergency;
        document.getElementById("emergencyStatus").style.color = "red";
      } else {
        resetEmergencyIndicators();
        document.getElementById("emergencyStatus").innerText = "NO";
        document.getElementById("emergencyStatus").style.color = "white";
      }

      // Update vehicle counts
      if(data.vehicles && data.vehicles.length === 4) {
        console.log("📊 Density data:", data.vehicles);
        console.log("🚗 Count data:", data.counts);
        updateDensityBadges(data.vehicles);
        
        // Update density graph
        vehicleChart.data.datasets[0].data = data.vehicles;
        vehicleChart.update('none');

        densityChart.data.datasets[0].data = data.vehicles;
        densityChart.update('none');

        // Calculate total from COUNTS (not density)
        if(data.counts && data.counts.length === 4) {
          vehicleTotal = data.counts.reduce((a, b) => a + b, 0);
        } else {
          vehicleTotal = data.vehicles.reduce((a, b) => a + b, 0);
        }
        document.getElementById("vehicleCount").innerText = vehicleTotal;

        // Find peak lane (highest density)
        let highest = Math.max(...data.vehicles);
        let peakLane = data.vehicles.indexOf(highest) + 1;
        
        // Only show peak if there's actual traffic (> 0%)
        if(highest > 0) {
          document.getElementById("peakLane").innerText = "Lane " + peakLane + " (" + highest + "%)";
        } else {
          document.getElementById("peakLane").innerText = "No Traffic";
        }
      } else {
        console.warn("⚠️ Vehicle data missing or invalid:", data.vehicles);
      }

      // Update street light status
      if(data.streetLight !== undefined) {
        if(data.streetLight === 1) {
          document.getElementById("streetLight").innerText = "ON";
          document.getElementById("streetLight").style.color = "yellow";
        } else {
          document.getElementById("streetLight").innerText = "OFF";
          document.getElementById("streetLight").style.color = "white";
        }
      }

      // Update LDR value
      if(data.ldrValue !== undefined) {
        document.getElementById("ldrValue").innerText = data.ldrValue;
      }

      // Update street light mode
      if(data.autoMode !== undefined) {
        if(data.autoMode === 1) {
          document.getElementById("streetMode").innerText = "AUTO MODE";
        } else {
          document.getElementById("streetMode").innerText = "MANUAL MODE";
        }
      }

      // Update traffic control mode
      if(data.manualMode !== undefined) {
        if(data.manualMode === 1) {
          document.getElementById("controlMode").innerText = "MANUAL";
          document.getElementById("controlMode").style.color = "#FFC107";
        } else {
          document.getElementById("controlMode").innerText = "AUTO";
          document.getElementById("controlMode").style.color = "#4CAF50";
        }
      }
    })
    .catch(error => {
      console.error("Error fetching data:", error);
      document.getElementById("connectionStatus").innerText = "OFFLINE";
      document.getElementById("connectionStatus").style.color = "#F44336";
    });
}

// ═══════════════════════════════════════════════════════════
// ⚙️ CONFIGURATION
// ═══════════════════════════════════════════════════════════
function saveConfig() {
  const newIP = document.getElementById("esp32IP").value;
  if(newIP && newIP.trim() !== "") {
    esp32IP = newIP.trim();
    localStorage.setItem("esp32IP", esp32IP);
    addLog("⚙️ Configuration saved! Connecting to " + esp32IP);
    
    // Test connection immediately
    fetchRealData();
  } else {
    addLog("❌ Invalid IP address");
  }
}

// ═══════════════════════════════════════════════════════════
// 🚀 INITIALIZATION
// ═══════════════════════════════════════════════════════════
// Fetch data every 1 second
setInterval(fetchRealData, 1000);

// Initial load
addLog("🚀 System started - Connecting to ESP32...");
addLog("📡 ESP32 IP: " + esp32IP);
fetchRealData();

// Set initial state
resetLights();
document.getElementById("r1").classList.add("active");
document.getElementById("r2").classList.add("active");
document.getElementById("r3").classList.add("active");
document.getElementById("r4").classList.add("active");
