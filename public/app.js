const firebaseConfig = {
  apiKey: "AIzaSyB2cz829BEMrwoxjuMeRF_m_2ibp79DJtw",
  authDomain: "duongleproject.firebaseapp.com",
  databaseURL: "https://duongleproject-default-rtdb.firebaseio.com",
  projectId: "duongleproject",
  storageBucket: "duongleproject.firebasestorage.app",
  messagingSenderId: "59471845114",
  appId: "1:59471845114:web:5a717b8014b864e4459b43",
  measurementId: "G-GP18L0L0XJ",
};

// Khởi tạo Firebase
firebase.initializeApp(firebaseConfig);
const database = firebase.database();
const auth = firebase.auth();

// Tham chiếu đến các node trong Realtime Database
const systemRef = database.ref('system');
const sensorsRef = database.ref('sensors');
const relaysRef = database.ref('relays');
const buttonsRef = database.ref('buttons');

// Hàm thiết lập cập nhật dữ liệu thời gian thực
function setupRealTimeUpdates() {
  // Theo dõi chế độ hệ thống
  systemRef.child('mode').on('value', (snapshot) => {
      const mode = snapshot.val();
      document.getElementById('system-mode').textContent = mode ? 'TỰ ĐỘNG' : 'THỦ CÔNG';
      document.getElementById('mode-toggle').textContent = mode ? 
          'CHUYỂN SANG CHẾ ĐỘ THỦ CÔNG' : 'CHUYỂN SANG CHẾ ĐỘ TỰ ĐỘNG';
      
      // Disable các nút điều khiển khi ở chế độ tự động
      document.getElementById('relay-switch').disabled = mode;
      document.getElementById('tip122-switch').disabled = mode;
  });

  // Theo dõi cảm biến độ ẩm đất
  sensorsRef.child('1/moisture').on('value', (snapshot) => {
      const moisture = snapshot.val();
      document.getElementById('moisture-value').textContent = moisture;
      document.getElementById('soil-moisture').textContent = moisture;
      updateGauge('moisture-gauge', moisture);
  });

  // Theo dõi cảm biến ánh sáng
  sensorsRef.child('2/light').on('value', (snapshot) => {
      const light = snapshot.val();
      document.getElementById('light-value').textContent = light;
      document.getElementById('light-level').textContent = light;
      updateGauge('light-gauge', light);
  });

  // Theo dõi nhiệt độ và độ ẩm không khí
  sensorsRef.child('1/temperature').on('value', (snapshot) => {
      document.getElementById('temp-value').textContent = snapshot.val().toFixed(1);
  });

  sensorsRef.child('1/humidity').on('value', (snapshot) => {
      document.getElementById('humi-value').textContent = snapshot.val().toFixed(1);
  });

  // Theo dõi trạng thái relay
  relaysRef.child('1').on('value', (snapshot) => {
      const state = snapshot.val();
      document.getElementById('relay-switch').checked = state;
      document.getElementById('relay-status').textContent = state ? 'ON' : 'OFF';
      document.getElementById('relay-status').className = state ? 'on' : 'off';
  });

  // Theo dõi trạng thái TIP122
  relaysRef.child('2').on('value', (snapshot) => {
      const state = snapshot.val();
      document.getElementById('tip122-switch').checked = state;
      document.getElementById('tip122-status').textContent = state ? 'ON' : 'OFF';
      document.getElementById('tip122-status').className = state ? 'on' : 'off';
  });
}

// Hàm cập nhật thanh gauge
function updateGauge(gaugeId, value) {
  const gauge = document.getElementById(gaugeId);
  const fill = gauge.querySelector('.gauge-fill') || document.createElement('div');
  fill.className = 'gauge-fill';
  fill.style.width = `${value}%`;
  if (!gauge.querySelector('.gauge-fill')) {
      gauge.appendChild(fill);
  }
}

// Hàm thiết lập trạng thái kết nối
function setupConnectionStatus() {
  const connectedRef = database.ref('.info/connected');
  connectedRef.on('value', (snapshot) => {
      document.getElementById('wifi-status').textContent = snapshot.val() ? 'Đã kết nối' : 'Mất kết nối';
      document.getElementById('wifi-status').style.color = snapshot.val() ? '#2ecc71' : '#e74c3c';
  });
}

// Kiểm tra trạng thái đăng nhập khi tải trang
window.addEventListener('load', () => {
  // Xóa sessionStorage khi làm mới trang để yêu cầu đăng nhập lại
  sessionStorage.removeItem('isLoggedIn');

  auth.onAuthStateChanged((user) => {
    if (user && sessionStorage.getItem('isLoggedIn')) {
      // Đã đăng nhập trong phiên hiện tại
      document.getElementById('login-container').style.display = 'none';
      document.querySelector('.container').style.display = 'block';
      setupRealTimeUpdates();
      setupConnectionStatus();
    } else {
      // Chưa đăng nhập hoặc làm mới trang
      document.getElementById('login-container').style.display = 'block';
      document.querySelector('.container').style.display = 'none';
      auth.signOut(); // Đăng xuất hoàn toàn để đảm bảo phải đăng nhập lại
    }
  });
});

// Xử lý sự kiện đăng nhập
document.getElementById('login-button').addEventListener('click', () => {
  const email = document.getElementById('email').value;
  const password = document.getElementById('password').value;
  
  auth.signInWithEmailAndPassword(email, password)
    .then(() => {
      // Lưu trạng thái đăng nhập trong sessionStorage
      sessionStorage.setItem('isLoggedIn', 'true');
      document.getElementById('login-container').style.display = 'none';
      document.querySelector('.container').style.display = 'block';
      setupRealTimeUpdates();
      setupConnectionStatus();
    })
    .catch((error) => {
      document.getElementById('login-error').textContent = "Sai email hoặc mật khẩu!";
      console.error("Lỗi đăng nhập:", error);
    });
});

// Xử lý sự kiện đăng xuất
document.getElementById('logout-button').addEventListener('click', () => {
  auth.signOut().then(() => {
    sessionStorage.removeItem('isLoggedIn');
    document.getElementById('login-container').style.display = 'block';
    document.querySelector('.container').style.display = 'none';
    console.log("Đăng xuất thành công");
  }).catch((error) => {
    console.error("Lỗi khi đăng xuất:", error);
  });
});

// Xử lý sự kiện chuyển chế độ
document.getElementById('mode-toggle').addEventListener('click', () => {
  systemRef.child('mode').once('value', (snapshot) => {
      const currentMode = snapshot.val();
      systemRef.child('mode').set(!currentMode);
  });
});

// Xử lý sự kiện bật/tắt relay
document.getElementById('relay-switch').addEventListener('change', (e) => {
  relaysRef.child('1').set(e.target.checked ? 1 : 0);
  buttonsRef.child('1').set(1);
  setTimeout(() => buttonsRef.child('1').set(0), 200);
});

// Xử lý sự kiện bật/tắt TIP122
document.getElementById('tip122-switch').addEventListener('change', (e) => {
  relaysRef.child('2').set(e.target.checked ? 1 : 0);
  buttonsRef.child('2').set(1);
  setTimeout(() => buttonsRef.child('2').set(0), 200);
});