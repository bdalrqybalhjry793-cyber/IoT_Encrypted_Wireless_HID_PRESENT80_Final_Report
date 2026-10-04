#include <Keyboard.h>

static const uint32_t UART_BAUD = 9600;

void sendAck(bool ok) {
  Serial1.println(ok ? "ACK" : "NACK");
}

bool processExpandedCommand(String command) {
  command.trim();

  // 1. أمر فتح نافذة Run العادية
  if (command == "OPEN_RUN") {
    Keyboard.press(KEY_LEFT_GUI); 
    Keyboard.press('r'); 
    delay(150); 
    Keyboard.releaseAll();
    return true;
  }

  // 2. أمر فتح CMD بصلاحيات الأدمن (Administrator Privilege Escalation)
  if (command == "OPEN_ADMIN_CMD") {
    // فتح نافذة التشغيل السريع Win + R
    Keyboard.press(KEY_LEFT_GUI); 
    Keyboard.press('r'); 
    delay(150); 
    Keyboard.releaseAll();
    delay(800);

    // كتابة أمر الاستدعاء المرفع عبر PowerShell
    Keyboard.print("powershell Start-Process cmd -Verb RunAs");
    delay(200);

    // تنفيذ الأمر
    Keyboard.write(KEY_RETURN);
    delay(1200); // انتظار ظهور نافذة UAC (User Account Control)

    // تجاوز نافذة UAC بالضغط على زر "نعم / Yes" عبر الاختصار Alt + Y
    Keyboard.press(KEY_LEFT_ALT);
    Keyboard.press('y');
    delay(150);
    Keyboard.releaseAll();

    // خيار إضافي لتأكيد UAC في حال عدم دعم Alt+Y (السهم الأيسر + Enter)
    delay(300);
    Keyboard.write(KEY_LEFT_ARROW);
    delay(100);
    Keyboard.write(KEY_RETURN);

    return true;
  }

  // 3. أمر مسح وتنظيف النص (Ctrl + A ثم Backspace)
  if (command == "CLEAR_TEXT") {
    Keyboard.press(KEY_LEFT_CTRL); 
    Keyboard.press('a'); 
    delay(100); 
    Keyboard.releaseAll();
    delay(100);
    Keyboard.write(KEY_BACKSPACE);
    return true;
  }

  // 4. أمر ضغط مفتاح Enter
  if (command == "ENTER") {
    Keyboard.write(KEY_RETURN);
    return true;
  }

  // 5. كتابة أي نص مخصص
  if (command.startsWith("STRING ")) {
    String textToType = command.substring(7);
    Keyboard.print(textToType);
    return true;
  }

  return false;
}

void setup() {
  Serial1.begin(UART_BAUD);
  Serial1.setTimeout(100);
  Keyboard.begin();
  delay(1000);
}

void loop() {
  if (Serial1.available()) {
    String command = Serial1.readStringUntil('\n');
    bool ok = processExpandedCommand(command);
    sendAck(ok);
  }
}