/*
  Teste de Dual-Core no ESP32 com FreeRTOS
  =========================================
  O ESP32 tem dois núcleos (Core 0 e Core 1).
  Com xTaskCreatePinnedToCore() é possível pinnar tasks em núcleos específicos.

  O que este teste faz:
  - Task 1 (Core 0): imprime "Core 0 - tick N" a cada 1 segundo
  - Task 2 (Core 1): imprime "Core 1 - tick N" a cada 1.5 segundos
  - loop() (Core 1): imprime o status a cada 5 segundos

  Se as duas tasks estiverem rodando de forma independente, você verá as
  mensagens intercaladas no Serial Monitor sem que uma bloqueie a outra.
*/

TaskHandle_t taskCore0Handle;
TaskHandle_t taskCore1Handle;

// Mutex para proteger o acesso ao Serial (evita mensagens misturadas)
SemaphoreHandle_t serialMutex;

volatile int tickCore0 = 0;
volatile int tickCore1 = 0;

// Task que roda no Core 0
void taskCore0(void* parameter) {
  Serial.print("Task Core 0 iniciada no core: ");
  Serial.println(xPortGetCoreID());

  while (true) {
    tickCore0++;

    xSemaphoreTake(serialMutex, portMAX_DELAY);
    Serial.print("[Core 0] tick ");
    Serial.println(tickCore0);
    xSemaphoreGive(serialMutex);

    vTaskDelay(pdMS_TO_TICKS(1000));  // aguarda 1 segundo
  }
}

// Task que roda no Core 1
void taskCore1(void* parameter) {
  Serial.print("Task Core 1 iniciada no core: ");
  Serial.println(xPortGetCoreID());

  while (true) {
    tickCore1++;

    xSemaphoreTake(serialMutex, portMAX_DELAY);
    Serial.print("          [Core 1] tick ");
    Serial.println(tickCore1);
    xSemaphoreGive(serialMutex);

    vTaskDelay(pdMS_TO_TICKS(1500));  // aguarda 1.5 segundos
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println("\n=== Teste Dual-Core ESP32 ===");
  Serial.print("Setup rodando no core: ");
  Serial.println(xPortGetCoreID());

  serialMutex = xSemaphoreCreateMutex();

  // xTaskCreatePinnedToCore(
  //   função,       nome,       stack,  parâmetro,  prioridade,  handle,   núcleo
  xTaskCreatePinnedToCore(taskCore0, "TaskCore0", 2048, NULL, 1, &taskCore0Handle, 0);
  xTaskCreatePinnedToCore(taskCore1, "TaskCore1", 2048, NULL, 1, &taskCore1Handle, 1);

  Serial.println("Tasks criadas. Aguardando ticks...\n");
}

void loop() {
  // loop() roda no Core 1 por padrão no Arduino/ESP32
  // Aqui mostramos que o loop principal continua rodando junto com as tasks
  Serial.print("[loop()] core=");
  Serial.print(xPortGetCoreID());
  Serial.print("  ticks Core0=");
  Serial.print(tickCore0);
  Serial.print("  ticks Core1=");
  Serial.println(tickCore1);

  delay(5000);  // imprime status a cada 5 segundos
}
