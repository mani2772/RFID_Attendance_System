#include <WiFi.h>
#include <SPI.h>
#include <MFRC522.h>
#include <WebServer.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

// Function prototypes
void handleRoot();
void handleAttendanceSheet();
void handleAccessDenied();
void handleAlreadyScanned();
void handleStatus();
String getTimestamp();

#define RST_PIN 22   // RC522 Reset pin
#define SS_PIN  21   // RC522 SDA pin
#define BUZZER_PIN  4  // Buzzer pin
#define LED_PIN     2  // LED pin

MFRC522 mfrc522(SS_PIN, RST_PIN); 
WebServer server(80);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

// WiFi credentials
const char* ssid = "Shang Chii";       
const char* password = "12345678";  

// Structure for storing user details
struct User {
  String uid;
  String name;
  String id;
  String timestamp;
  bool scanned;
};

// Predefined users
User users[] = {
  {"633D2314", "Bhargav Ram", "BT22ECE107", "", false},
  {"23078DDC", "Manikanta", "BT22ECE106", "", false},
  {"E305FB27", "Shiva", "BT22ECE104", "", false},
  {"831892F5", "Amanikanta", "BT22ECE105", "", false}
};

const int userCount = sizeof(users) / sizeof(users[0]);
String lastMessage = "Ready to scan cards...";
String lastUID = "";
String lastStatus = "";
String currentUserName = "";
String currentUserID = "";
bool newScan = false;

void setup() {
  Serial.begin(115200);
  SPI.begin();
  mfrc522.PCD_Init();

  WiFi.begin(ssid, password);
  Serial.println("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi! IP address: ");
  Serial.println(WiFi.localIP());

  timeClient.begin();
  timeClient.setTimeOffset(19800); // UTC+5:30 for IST

  server.on("/", handleRoot);
  server.on("/attendance", handleAttendanceSheet);
  server.on("/access-denied", handleAccessDenied);
  server.on("/already-scanned", handleAlreadyScanned);
  server.on("/status", handleStatus);
  server.begin();
  Serial.println("Web server started");

  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);

  Serial.println("System initialized. Ready to scan cards...");
}

void loop() {
  server.handleClient();
  timeClient.update();

  if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) {
    return;
  }

  String scannedUID = "";
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    scannedUID += String(mfrc522.uid.uidByte[i], HEX);
  }
  scannedUID.toUpperCase();
  
  Serial.print("Scanned UID: ");
  Serial.println(scannedUID);

  bool knownUser = false;
  bool alreadyScanned = false;
  User* currentUser = nullptr;

  for (int i = 0; i < userCount; i++) {
    if (scannedUID == users[i].uid) {
      knownUser = true;
      currentUser = &users[i];
      currentUserName = users[i].name;
      currentUserID = users[i].id;
      if (users[i].scanned) {
        alreadyScanned = true;
      } else {
        users[i].timestamp = getTimestamp();
        users[i].scanned = true;
        newScan = true;
      }
      break;
    }
  }

  lastUID = scannedUID;
  
  if (knownUser) {
    if (alreadyScanned) {
      lastMessage = "ALREADY SCANNED: " + currentUser->name + " (" + currentUser->id + ")";
      lastStatus = "already";
      tone(BUZZER_PIN, 800, 200);
      delay(100);
      tone(BUZZER_PIN, 800, 200);
      server.sendHeader("Location", "/already-scanned");
      server.send(303);
    } else {
      lastMessage = "ACCESS GRANTED: " + currentUser->name + " (" + currentUser->id + ")";
      lastStatus = "granted";
      tone(BUZZER_PIN, 1000, 300);
    }
    digitalWrite(LED_PIN, HIGH);
    delay(300);
    digitalWrite(LED_PIN, LOW);
  } else {
    lastMessage = "NO ACCESS: Unknown card " + scannedUID;
    lastStatus = "denied";
    tone(BUZZER_PIN, 300, 500);
    digitalWrite(LED_PIN, HIGH);
    delay(500);
    digitalWrite(LED_PIN, LOW);
    server.sendHeader("Location", "/access-denied");
    server.send(303);
  }

  Serial.println(lastMessage);
}

String getTimestamp() {
  timeClient.update();
  unsigned long epochTime = timeClient.getEpochTime();
  struct tm *ptm = gmtime ((time_t *)&epochTime); 

  int monthDay = ptm->tm_mday;
  int currentMonth = ptm->tm_mon+1;
  int currentYear = ptm->tm_year+1900;
  String currentDate = String(currentYear) + "-" + String(currentMonth) + "-" + String(monthDay);

  String hours = String(timeClient.getHours());
  String minutes = String(timeClient.getMinutes());
  String seconds = String(timeClient.getSeconds());
  
  if (hours.length() == 1) hours = "0" + hours;
  if (minutes.length() == 1) minutes = "0" + minutes;
  if (seconds.length() == 1) seconds = "0" + seconds;

  return currentDate + " " + hours + ":" + minutes + ":" + seconds;
}

void handleRoot() {
  String currentTime = getTimestamp();
  
  String page = R"=====(
<!DOCTYPE html>
<html>
<head>
<title>RFID Attendance System</title>
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<style>
  @import url('https://fonts.googleapis.com/css2?family=Poppins:wght@300;400;600;700&display=swap');
  
  :root {
    --primary: #6c5ce7;
    --secondary: #a29bfe;
    --success: #00b894;
    --warning: #fdcb6e;
    --danger: #d63031;
    --light: #f5f6fa;
    --dark: #2d3436;
    --white: #ffffff;
  }
  
  body {
    font-family: 'Poppins', sans-serif;
    margin: 0;
    padding: 0;
    display: flex;
    justify-content: center;
    align-items: center;
    min-height: 100vh;
    background: linear-gradient(135deg, var(--primary), var(--secondary));
    color: var(--dark);
  }
  
  .card {
    width: 90%;
    max-width: 500px;
    background: rgba(255, 255, 255, 0.95);
    border-radius: 20px;
    padding: 2rem;
    box-shadow: 0 10px 30px rgba(0, 0, 0, 0.2);
    backdrop-filter: blur(10px);
    border: 1px solid rgba(255, 255, 255, 0.2);
    animation: fadeIn 0.5s ease-out;
  }
  
  @keyframes fadeIn {
    from { opacity: 0; transform: translateY(20px); }
    to { opacity: 1; transform: translateY(0); }
  }
  
  h1 {
    color: var(--primary);
    text-align: center;
    margin-bottom: 1.5rem;
    font-weight: 700;
    font-size: 2rem;
    background: linear-gradient(to right, var(--primary), var(--secondary));
    -webkit-background-clip: text;
    -webkit-text-fill-color: transparent;
  }
  
  .time-display {
    background: var(--white);
    padding: 0.8rem;
    border-radius: 10px;
    margin-bottom: 1.5rem;
    text-align: center;
    font-weight: 600;
    color: var(--primary);
    box-shadow: 0 4px 6px rgba(0, 0, 0, 0.1);
  }
  
  .status-box {
    padding: 1.5rem;
    margin: 1.5rem 0;
    border-radius: 12px;
    text-align: center;
    font-weight: 500;
    box-shadow: 0 4px 15px rgba(0, 0, 0, 0.1);
    transition: all 0.3s ease;
    border-left: 5px solid;
  }
  
  .status-default {
    background: linear-gradient(135deg, #f5f7fa, #c3cfe2);
    border-left-color: var(--primary);
  }
  
  .status-granted {
    background: linear-gradient(135deg, #d4edda, #c3e6cb);
    border-left-color: var(--success);
  }
  
  .btn {
    display: inline-block;
    padding: 0.8rem 1.8rem;
    margin: 0.5rem;
    border-radius: 50px;
    font-weight: 600;
    text-decoration: none;
    text-align: center;
    transition: all 0.3s ease;
    box-shadow: 0 4px 15px rgba(0, 0, 0, 0.1);
    border: none;
    cursor: pointer;
  }
  
  .btn-primary {
    background: linear-gradient(135deg, var(--primary), var(--secondary));
    color: var(--white);
  }
  
  .btn-primary:hover {
    transform: translateY(-3px);
    box-shadow: 0 8px 25px rgba(108, 92, 231, 0.3);
  }
  
  .scan-icon {
    font-size: 3rem;
    text-align: center;
    margin: 1rem 0;
    color: var(--primary);
  }
  
  .instruction {
    text-align: center;
    color: var(--dark);
    margin-bottom: 1.5rem;
    font-size: 1.1rem;
  }
</style>
<script>
  function updateStatus() {
    fetch('/status')
      .then(response => response.json())
      .then(data => {
        // Update status box appearance
        const statusBox = document.getElementById('status-box');
        statusBox.className = 'status-box status-' + (data.status || 'default');
        document.getElementById('status-message').innerHTML = data.message;
      });
  }
  setInterval(updateStatus, 1000);
</script>
</head>
<body>
<div class="card">
  <h1>RFID Attendance System</h1>
  <div class="time-display" id="datetime">)=====" + currentTime + R"=====(</div>
  <div class="scan-icon">⌨</div>
  <p class="instruction">Scan your ID card to mark attendance</p>
  <div id="status-box" class="status-box status-default">
    <div id="status-message">)=====" + lastMessage + R"=====(</div>
  </div>
  <div style="text-align: center;">
    <a href="/attendance" class="btn btn-primary">View Attendance</a>
  </div>
</div>
<script>
  function updateDateTime() {
    const now = new Date();
    const datetimeElement = document.getElementById('datetime');
    datetimeElement.textContent = now.toLocaleString();
  }
  setInterval(updateDateTime, 1000);
</script>
</body>
</html>
)=====";
  server.send(200, "text/html", page);
}

void handleAttendanceSheet() {
  String currentTime = getTimestamp();
  
  String page = R"=====(
<!DOCTYPE html>
<html>
<head>
<title>Attendance Sheet</title>
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<style>
  @import url('https://fonts.googleapis.com/css2?family=Poppins:wght@300;400;600;700&display=swap');
  
  :root {
    --primary: #6c5ce7;
    --secondary: #a29bfe;
    --success: #00b894;
    --white: #ffffff;
    --light: #f5f6fa;
    --dark: #2d3436;
  }
  
  body {
    font-family: 'Poppins', sans-serif;
    margin: 0;
    padding: 20px;
    background: linear-gradient(135deg, var(--primary), var(--secondary));
  }
  
  .container {
    max-width: 800px;
    margin: 20px auto;
    background: rgba(255, 255, 255, 0.95);
    padding: 2rem;
    border-radius: 20px;
    box-shadow: 0 10px 30px rgba(0, 0, 0, 0.2);
    animation: fadeIn 0.5s ease-out;
  }
  
  h1 {
    color: var(--primary);
    text-align: center;
    margin-bottom: 1.5rem;
    font-weight: 700;
    background: linear-gradient(to right, var(--primary), var(--secondary));
    -webkit-background-clip: text;
    -webkit-text-fill-color: transparent;
  }
  
  .time-display {
    background: var(--white);
    padding: 0.8rem;
    border-radius: 10px;
    margin-bottom: 1.5rem;
    text-align: center;
    font-weight: 600;
    color: var(--primary);
    box-shadow: 0 4px 6px rgba(0, 0, 0, 0.1);
  }
  
  table {
    width: 100%;
    border-collapse: collapse;
    margin: 1.5rem 0;
    box-shadow: 0 5px 15px rgba(0, 0, 0, 0.1);
    border-radius: 12px;
    overflow: hidden;
  }
  
  th {
    background: linear-gradient(135deg, var(--primary), var(--secondary));
    color: var(--white);
    padding: 1rem;
    text-align: left;
    font-weight: 600;
  }
  
  td {
    padding: 1rem;
    border-bottom: 1px solid #eee;
  }
  
  tr:nth-child(even) {
    background-color: var(--light);
  }
  
  tr:hover {
    background-color: rgba(108, 92, 231, 0.1);
  }
  
  .btn {
    display: inline-block;
    padding: 0.8rem 1.8rem;
    margin: 1rem 0.5rem 0;
    border-radius: 50px;
    font-weight: 600;
    text-decoration: none;
    transition: all 0.3s ease;
    box-shadow: 0 4px 15px rgba(0, 0, 0, 0.1);
  }
  
  .btn-primary {
    background: linear-gradient(135deg, var(--primary), var(--secondary));
    color: var(--white);
  }
  
  .btn-primary:hover {
    transform: translateY(-3px);
    box-shadow: 0 8px 25px rgba(108, 92, 231, 0.3);
  }
  
  .empty-state {
    text-align: center;
    padding: 2rem;
    color: var(--dark);
  }
</style>
</head>
<body>
<div class="container">
  <h1>Attendance Sheet</h1>
  <div class="time-display">Current Time: )=====" + currentTime + R"=====(</div>
  <table>
    <thead>
      <tr>
        <th>Name</th>
        <th>ID</th>
        <th>Timestamp</th>
      </tr>
    </thead>
    <tbody>
)=====";

  bool hasAttendance = false;
  for (int i = 0; i < userCount; i++) {
    if (users[i].scanned) {
      page += "<tr><td>" + users[i].name + "</td><td>" + users[i].id + "</td><td>" + users[i].timestamp + "</td></tr>";
      hasAttendance = true;
    }
  }

  if (!hasAttendance) {
    page += "<tr><td colspan='3' class='empty-state'>No attendance records yet</td></tr>";
  }

  page += R"=====(
    </tbody>
  </table>
  <div style="text-align: center;">
    <a href="/" class="btn btn-primary">Back to Home</a>
  </div>
</div>
</body>
</html>
)=====";
  server.send(200, "text/html", page);
}

void handleAccessDenied() {
  String page = R"=====(
<!DOCTYPE html>
<html>
<head>
<title>Access Denied</title>
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<style>
  @import url('https://fonts.googleapis.com/css2?family=Poppins:wght@300;400;600;700&display=swap');
  
  :root {
    --danger: #d63031;
    --danger-light: #ff7675;
    --white: #ffffff;
    --dark: #2d3436;
  }
  
  body {
    font-family: 'Poppins', sans-serif;
    margin: 0;
    padding: 0;
    display: flex;
    justify-content: center;
    align-items: center;
    min-height: 100vh;
    background: linear-gradient(135deg, var(--danger), var(--danger-light));
  }
  
  .card {
    width: 90%;
    max-width: 500px;
    background: rgba(255, 255, 255, 0.95);
    border-radius: 20px;
    padding: 2rem;
    box-shadow: 0 10px 30px rgba(0, 0, 0, 0.2);
    text-align: center;
    animation: fadeIn 0.5s ease-out;
  }
  
  h1 {
    color: var(--danger);
    margin-bottom: 1.5rem;
    font-weight: 700;
  }
  
  .denied-icon {
    font-size: 4rem;
    color: var(--danger);
    margin: 1rem 0;
    animation: pulse 1.5s infinite;
  }
  
  @keyframes pulse {
    0% { transform: scale(1); }
    50% { transform: scale(1.1); }
    100% { transform: scale(1); }
  }
  
  p {
    margin: 0.8rem 0;
    color: var(--dark);
  }
  
  .btn {
    display: inline-block;
    padding: 0.8rem 1.8rem;
    margin: 0.5rem;
    border-radius: 50px;
    font-weight: 600;
    text-decoration: none;
    transition: all 0.3s ease;
    box-shadow: 0 4px 15px rgba(0, 0, 0, 0.1);
  }
  
  .btn-danger {
    background: linear-gradient(135deg, var(--danger), var(--danger-light));
    color: var(--white);
  }
  
  .btn-danger:hover {
    transform: translateY(-3px);
    box-shadow: 0 8px 25px rgba(214, 48, 49, 0.3);
  }
  
  .btn-light {
    background: var(--white);
    color: var(--danger);
    border: 2px solid var(--danger);
  }
  
  .btn-light:hover {
    background: var(--danger);
    color: var(--white);
  }
</style>
</head>
<body>
<div class="card">
  <div class="denied-icon">✖</div>
  <h1>ACCESS DENIED</h1>
  <p>Card UID: )=====" + lastUID + R"=====(</p>
  <p>This card is not registered in the system</p>
  <div style="margin-top: 2rem;">
    <a href="/" class="btn btn-light">Back to Home</a>
    <a href="/attendance" class="btn btn-danger">View Attendance</a>
  </div>
</div>
</body>
</html>
)=====";
  server.send(200, "text/html", page);
}

void handleAlreadyScanned() {
  String page = R"=====(
<!DOCTYPE html>
<html>
<head>
<title>Already Scanned</title>
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<style>
  @import url('https://fonts.googleapis.com/css2?family=Poppins:wght@300;400;600;700&display=swap');
  
  :root {
    --warning: #fdcb6e;
    --warning-dark: #e17055;
    --white: #ffffff;
    --dark: #2d3436;
  }
  
  body {
    font-family: 'Poppins', sans-serif;
    margin: 0;
    padding: 0;
    display: flex;
    justify-content: center;
    align-items: center;
    min-height: 100vh;
    background: linear-gradient(135deg, var(--warning), var(--warning-dark));
  }
  
  .card {
    width: 90%;
    max-width: 500px;
    background: rgba(255, 255, 255, 0.95);
    border-radius: 20px;
    padding: 2rem;
    box-shadow: 0 10px 30px rgba(0, 0, 0, 0.2);
    text-align: center;
    animation: fadeIn 0.5s ease-out;
  }
  
  h1 {
    color: var(--warning-dark);
    margin-bottom: 1.5rem;
    font-weight: 700;
  }
  
  .warning-icon {
    font-size: 4rem;
    color: var(--warning-dark);
    margin: 1rem 0;
    animation: bounce 1.5s infinite;
  }
  
  @keyframes bounce {
    0%, 100% { transform: translateY(0); }
    50% { transform: translateY(-10px); }
  }
  
  .user-info {
    background: rgba(253, 203, 110, 0.2);
    padding: 1rem;
    border-radius: 10px;
    margin: 1rem 0;
  }
  
  .btn {
    display: inline-block;
    padding: 0.8rem 1.8rem;
    margin: 0.5rem;
    border-radius: 50px;
    font-weight: 600;
    text-decoration: none;
    transition: all 0.3s ease;
    box-shadow: 0 4px 15px rgba(0, 0, 0, 0.1);
  }
  
  .btn-warning {
    background: linear-gradient(135deg, var(--warning), var(--warning-dark));
    color: var(--white);
  }
  
  .btn-warning:hover {
    transform: translateY(-3px);
    box-shadow: 0 8px 25px rgba(253, 203, 110, 0.3);
  }
  
  .btn-light {
    background: var(--white);
    color: var(--warning-dark);
    border: 2px solid var(--warning-dark);
  }
  
  .btn-light:hover {
    background: var(--warning-dark);
    color: var(--white);
  }
</style>
</head>
<body>
<div class="card">
  <div class="warning-icon">⚠</div>
  <h1>ALREADY SCANNED</h1>
  <div class="user-info">
    <p style="font-weight: 600; margin-bottom: 0.5rem;">)=====" + currentUserName + R"=====(</p>
    <p style="color: var(--warning-dark);">)=====" + currentUserID + R"=====(</p>
  </div>
  <p>This card has already been scanned today</p>
  <div style="margin-top: 2rem;">
    <a href="/" class="btn btn-light">Back to Home</a>
    <a href="/attendance" class="btn btn-warning">View Attendance</a>
  </div>
</div>
</body>
</html>
)=====";
  server.send(200, "text/html", page);
}

void handleStatus() {
  String json = "{";
  json += "\"status\":\"" + lastStatus + "\",";
  json += "\"message\":\"" + lastMessage + "\",";
  json += "\"uid\":\"" + lastUID + "\"";
  json += "}";
  
  server.send(200, "application/json", json);
}