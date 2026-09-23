#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>

#include "esp_lcd_panel_rgb.h"
#include "esp_lcd_panel_ops.h"

#define LCD_WIDTH   640
#define LCD_HEIGHT  480

// -------------------------------------------------------
// SINCRONISMO
// -------------------------------------------------------

#define PIN_PCLK   42
#define PIN_HSYNC  41
#define PIN_VSYNC  40
#define PIN_DE     39

// -------------------------------------------------------
// RGB565
//
// D0..D4   = Azul
// D5..D10  = Verde
// D11..D15 = Vermelho
//
// O G0 da sua tela não apareceu no chicote.
// Portanto GPIO9 será configurado pelo periférico,
// mas NÃO precisa ser conectado fisicamente agora.
// -------------------------------------------------------

#define PIN_B1   4
#define PIN_B2   5
#define PIN_B3   6
#define PIN_B4   7
#define PIN_B5   8

#define PIN_G0   9       // sem ligação física por enquanto
#define PIN_G1   10
#define PIN_G2   11
#define PIN_G3   12
#define PIN_G4   13
#define PIN_G5   14

#define PIN_R1   15
#define PIN_R2   16
#define PIN_R3   17
#define PIN_R4   18
#define PIN_R5   21

esp_lcd_panel_handle_t lcd_panel = nullptr;
uint16_t *framebuffer = nullptr;

constexpr char WIFI_SSID[] = "COMPANIA";
constexpr char WIFI_SENHA[] = "jesusateu123";
constexpr int ALTURA_CABECALHO = 42;

WebServer servidor(80);
String enderecoIp = "0.0.0.0";



// =======================================================
// PREENCHE A TELA COM UMA COR RGB565
// =======================================================

void preencherTela(uint16_t cor)
{
    if (framebuffer == nullptr) {
        return;
    }

    const size_t pixels = LCD_WIDTH * LCD_HEIGHT;

    for (size_t i = 0; i < pixels; i++) {
        framebuffer[i] = cor;
    }
}

void preencherRetangulo(int x, int y, int largura, int altura, uint16_t cor)
{
    const int inicioX = x < 0 ? 0 : x;
    const int inicioY = y < 0 ? 0 : y;
    const int fimX = x + largura > LCD_WIDTH ? LCD_WIDTH : x + largura;
    const int fimY = y + altura > LCD_HEIGHT ? LCD_HEIGHT : y + altura;

    for (int posY = inicioY; posY < fimY; posY++) {
        for (int posX = inicioX; posX < fimX; posX++) {
            framebuffer[posY * LCD_WIDTH + posX] = cor;
        }
    }
}

void preencherCirculo(int centroX, int centroY, int raio, uint16_t cor)
{
    const int raioAoQuadrado = raio * raio;

    for (int y = -raio; y <= raio; y++) {
        for (int x = -raio; x <= raio; x++) {
            if (x * x + y * y > raioAoQuadrado) {
                continue;
            }

            const int posX = centroX + x;
            const int posY = centroY + y;

            if (posX >= 0 && posX < LCD_WIDTH &&
                posY >= 0 && posY < LCD_HEIGHT) {
                framebuffer[posY * LCD_WIDTH + posX] = cor;
            }
        }
    }
}

int limitar(int valor, int minimo, int maximo)
{
    if (valor < minimo) {
        return minimo;
    }

    return valor > maximo ? maximo : valor;
}

void desenharPonto(int x, int y, uint16_t cor, int tamanho)
{
    preencherCirculo(x, y, tamanho / 2, cor);
}

void desenharLinha(int x0, int y0, int x1, int y1, uint16_t cor, int tamanho)
{
    const int deltaX = abs(x1 - x0);
    const int passoX = x0 < x1 ? 1 : -1;
    const int deltaY = -abs(y1 - y0);
    const int passoY = y0 < y1 ? 1 : -1;
    int erro = deltaX + deltaY;

    while (true) {
        desenharPonto(x0, y0, cor, tamanho);

        if (x0 == x1 && y0 == y1) {
            return;
        }

        const int erroDuplo = erro * 2;

        if (erroDuplo >= deltaY) {
            erro += deltaY;
            x0 += passoX;
        }

        if (erroDuplo <= deltaX) {
            erro += deltaX;
            y0 += passoY;
        }
    }
}

int indiceGlifo(char caractere)
{
    if (caractere >= '0' && caractere <= '9') {
        return caractere - '0';
    }

    switch (caractere) {
        case 'I': return 10;
        case 'P': return 11;
        case ':': return 12;
        case '.': return 13;
        default: return -1;
    }
}

void desenharTextoIp(const String &texto)
{
    constexpr uint8_t FONTE[][5] = {
        {0x3E, 0x51, 0x49, 0x45, 0x3E}, {0x00, 0x42, 0x7F, 0x40, 0x00},
        {0x42, 0x61, 0x51, 0x49, 0x46}, {0x21, 0x41, 0x45, 0x4B, 0x31},
        {0x18, 0x14, 0x12, 0x7F, 0x10}, {0x27, 0x45, 0x45, 0x45, 0x39},
        {0x3C, 0x4A, 0x49, 0x49, 0x30}, {0x01, 0x71, 0x09, 0x05, 0x03},
        {0x36, 0x49, 0x49, 0x49, 0x36}, {0x06, 0x49, 0x49, 0x29, 0x1E},
        {0x00, 0x41, 0x7F, 0x41, 0x00}, {0x7F, 0x09, 0x09, 0x09, 0x06},
        {0x00, 0x36, 0x36, 0x00, 0x00}, {0x00, 0x60, 0x60, 0x00, 0x00},
    };
    constexpr int ESCALA = 3;
    int x = 7;

    for (const char *caractere = texto.c_str(); *caractere != '\0'; caractere++) {
        const int indice = indiceGlifo(*caractere);

        if (indice < 0) {
            x += 3 * ESCALA;
            continue;
        }

        for (int coluna = 0; coluna < 5; coluna++) {
            for (int linha = 0; linha < 7; linha++) {
                if (FONTE[indice][coluna] & (1 << linha)) {
                    preencherRetangulo(
                        x + coluna * ESCALA,
                        10 + linha * ESCALA,
                        ESCALA,
                        ESCALA,
                        0xFFFF
                    );
                }
            }
        }

        x += 6 * ESCALA;
    }
}

void limparDesenho()
{
    preencherTela(0xFFFF);
    preencherRetangulo(0, 0, LCD_WIDTH, ALTURA_CABECALHO, 0x0000);
    desenharTextoIp("IP: " + enderecoIp);
}

void tratarLinha()
{
    if (framebuffer == nullptr ||
        !servidor.hasArg("x0") || !servidor.hasArg("y0") ||
        !servidor.hasArg("x1") || !servidor.hasArg("y1") ||
        !servidor.hasArg("c") || !servidor.hasArg("s")) {
        servidor.send(400, "text/plain", "Parametros invalidos");
        return;
    }

    const int x0 = limitar(servidor.arg("x0").toInt(), 0, LCD_WIDTH - 1);
    const int y0 = limitar(servidor.arg("y0").toInt(), 0, LCD_HEIGHT - ALTURA_CABECALHO - 1);
    const int x1 = limitar(servidor.arg("x1").toInt(), 0, LCD_WIDTH - 1);
    const int y1 = limitar(servidor.arg("y1").toInt(), 0, LCD_HEIGHT - ALTURA_CABECALHO - 1);
    const uint16_t cor = static_cast<uint16_t>(servidor.arg("c").toInt());
    const int tamanho = limitar(servidor.arg("s").toInt(), 1, 24);

    desenharLinha(
        x0,
        y0 + ALTURA_CABECALHO,
        x1,
        y1 + ALTURA_CABECALHO,
        cor,
        tamanho
    );
    servidor.send(204);
}

const char PAGINA_DESENHO[] PROGMEM = R"HTML(
<!doctype html><html lang="pt-BR"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Desenhar na Tela</title><style>
body{margin:0;background:#111827;color:#f9fafb;font:16px system-ui;display:grid;place-items:center;min-height:100vh}
main{width:min(94vw,700px);padding:16px;background:#1f2937;border-radius:14px;box-shadow:0 16px 50px #0008}
#topo{display:flex;gap:12px;align-items:center;flex-wrap:wrap;margin-bottom:12px}strong{margin-right:auto}button,input{accent-color:#ef4444}button{border:0;border-radius:7px;padding:8px 12px;font-weight:700;cursor:pointer}
canvas{width:100%;height:auto;background:#fff;border-radius:8px;touch-action:none;display:block}
</style><main><div id="topo"><strong>ESP: <span id="ip"></span></strong><label>Cor <input id="cor" type="color" value="#e11d48"></label><label>Pincel <input id="tam" type="range" min="1" max="24" value="4"></label><button id="limpar">Limpar</button></div><canvas id="tela" width="640" height="438"></canvas></main>
<script>
const tela=document.querySelector('#tela'),ctx=tela.getContext('2d'),cor=document.querySelector('#cor'),tam=document.querySelector('#tam');
document.querySelector('#ip').textContent=location.host;ctx.fillStyle='#fff';ctx.fillRect(0,0,640,438);let anterior=null;
function ponto(e){const r=tela.getBoundingClientRect();return{x:Math.round((e.clientX-r.left)*640/r.width),y:Math.round((e.clientY-r.top)*438/r.height)}}
function rgb565(h){const n=parseInt(h.slice(1),16),r=n>>16,g=n>>8&255,b=n&255;return((r&248)<<8)|((g&252)<<3)|(b>>3)}
function linha(a,b){ctx.strokeStyle=cor.value;ctx.lineWidth=tam.value;ctx.lineCap='round';ctx.beginPath();ctx.moveTo(a.x,a.y);ctx.lineTo(b.x,b.y);ctx.stroke();fetch(`/line?x0=${a.x}&y0=${a.y}&x1=${b.x}&y1=${b.y}&c=${rgb565(cor.value)}&s=${tam.value}`,{method:'POST'}).catch(()=>{})}
tela.addEventListener('pointerdown',e=>{anterior=ponto(e);tela.setPointerCapture(e.pointerId);linha(anterior,anterior)});
tela.addEventListener('pointermove',e=>{if(!anterior)return;const atual=ponto(e);linha(anterior,atual);anterior=atual});
tela.addEventListener('pointerup',()=>anterior=null);tela.addEventListener('pointercancel',()=>anterior=null);
document.querySelector('#limpar').onclick=()=>{fetch('/clear',{method:'POST'});ctx.fillStyle='#fff';ctx.fillRect(0,0,640,438)};
</script></html>
)HTML";

void iniciarWifiWeb()
{
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.begin(WIFI_SSID, WIFI_SENHA);

    const uint32_t inicio = millis();

    while (WiFi.status() != WL_CONNECTED && millis() - inicio < 20000) {
        delay(250);
    }

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("ERRO: Wi-Fi nao conectou.");
        return;
    }

    enderecoIp = WiFi.localIP().toString();
    servidor.on("/", HTTP_GET, []() {
        servidor.send_P(200, "text/html; charset=utf-8", PAGINA_DESENHO);
    });
    servidor.on("/line", HTTP_POST, tratarLinha);
    servidor.on("/clear", HTTP_POST, []() {
        if (framebuffer == nullptr) {
            servidor.send(503, "text/plain", "Tela indisponivel");
            return;
        }

        limparDesenho();
        servidor.send(204);
    });
    servidor.begin();
    Serial.printf("Painel web: http://%s\n", enderecoIp.c_str());
}


// =======================================================
// INICIALIZAÇÃO
// =======================================================

void setup()
{
    Serial.begin(115200);

    delay(2000);

    Serial.println();
    Serial.println("==============================");
    Serial.println(" Sharp LQ10D368 + ESP32-S3");
    Serial.println("==============================");

    Serial.print("PSRAM detectada: ");
    Serial.print(ESP.getPsramSize() / 1024 / 1024);
    Serial.println(" MB");

    if (ESP.getPsramSize() == 0) {
        Serial.println("ERRO: PSRAM nao detectada!");
        Serial.println("Confira Tools -> PSRAM -> OPI PSRAM");

        while (true) {
            delay(1000);
        }
    }

    Serial.println("Conectando ao Wi-Fi...");
    iniciarWifiWeb();


    // ===================================================
    // CONFIGURAÇÃO RGB
    // ===================================================

    esp_lcd_rgb_panel_config_t config = {};

    config.clk_src = LCD_CLK_SRC_DEFAULT;

    // RGB565 = barramento de 16 bits
    config.data_width = 16;

    // Um framebuffer
    config.num_fbs = 1;

    // Buffer intermediário em SRAM interna para reduzir artefatos da PSRAM.
    config.bounce_buffer_size_px = LCD_WIDTH * 10;

    // A tela não possui pino DISP separado
    config.disp_gpio_num = -1;

    config.pclk_gpio_num  = PIN_PCLK;
    config.hsync_gpio_num = PIN_HSYNC;
    config.vsync_gpio_num = PIN_VSYNC;
    config.de_gpio_num    = PIN_DE;


    // ===================================================
    // DADOS RGB565
    // ===================================================

    config.data_gpio_nums[0]  = PIN_B1;
    config.data_gpio_nums[1]  = PIN_B2;
    config.data_gpio_nums[2]  = PIN_B3;
    config.data_gpio_nums[3]  = PIN_B4;
    config.data_gpio_nums[4]  = PIN_B5;

    config.data_gpio_nums[5]  = PIN_G0;
    config.data_gpio_nums[6]  = PIN_G1;
    config.data_gpio_nums[7]  = PIN_G2;
    config.data_gpio_nums[8]  = PIN_G3;
    config.data_gpio_nums[9]  = PIN_G4;
    config.data_gpio_nums[10] = PIN_G5;

    config.data_gpio_nums[11] = PIN_R1;
    config.data_gpio_nums[12] = PIN_R2;
    config.data_gpio_nums[13] = PIN_R3;
    config.data_gpio_nums[14] = PIN_R4;
    config.data_gpio_nums[15] = PIN_R5;


    // ===================================================
    // TIMING DA SHARP LQ10D368
    // ===================================================

    config.timings.pclk_hz = 25180000;

    config.timings.h_res = LCD_WIDTH;
    config.timings.v_res = LCD_HEIGHT;


    // Horizontal:
    //
    // ENAB em LOW: primeiro pixel no clock C104.
    // 96 + 8 + 640 + 56 = 800 clocks
    //

    config.timings.hsync_pulse_width = 96;
    config.timings.hsync_back_porch  = 8;
    config.timings.hsync_front_porch = 56;


    // Vertical:
    //
    // 2 + 32 + 480 + 11 = 525 linhas
    //

    config.timings.vsync_pulse_width = 2;
    config.timings.vsync_back_porch  = 32;
    config.timings.vsync_front_porch = 11;


    // HSYNC e VSYNC negativos para modo 640x480
    config.timings.flags.hsync_idle_low = 0;
    config.timings.flags.vsync_idle_low = 0;
    config.timings.flags.de_idle_high = 0;

    // Dados mudam na borda de descida,
    // ficando estáveis para a outra borda
    config.timings.flags.pclk_active_neg = 1;

    config.timings.flags.pclk_idle_high = 0;


    // Framebuffer na PSRAM
    config.flags.fb_in_psram = 1;


    Serial.println("Criando painel RGB...");

    esp_err_t err =
        esp_lcd_new_rgb_panel(&config, &lcd_panel);

    if (err != ESP_OK) {
        Serial.printf(
            "ERRO esp_lcd_new_rgb_panel: %s\n",
            esp_err_to_name(err)
        );

        while (true) {
            delay(1000);
        }
    }

    Serial.println("Inicializando painel...");

    err = esp_lcd_panel_reset(lcd_panel);

    if (err != ESP_OK) {
        Serial.printf(
            "Erro reset: %s\n",
            esp_err_to_name(err)
        );
    }


    err = esp_lcd_panel_init(lcd_panel);

    if (err != ESP_OK) {
        Serial.printf(
            "ERRO init: %s\n",
            esp_err_to_name(err)
        );

        while (true) {
            delay(1000);
        }
    }


    // ===================================================
    // PEGA O FRAMEBUFFER CRIADO PELO DRIVER
    // ===================================================

    void *fb = nullptr;

    err = esp_lcd_rgb_panel_get_frame_buffer(
        lcd_panel,
        1,
        &fb
    );

    if (err != ESP_OK || fb == nullptr) {
        Serial.println("ERRO ao obter framebuffer.");

        while (true) {
            delay(1000);
        }
    }

    framebuffer = (uint16_t *)fb;


    Serial.printf(
        "Framebuffer: %p\n",
        framebuffer
    );

    Serial.println();
    Serial.println("LCD inicializado!");

    limparDesenho();
}

                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            
// =======================================================
// TESTE
// =======================================================

void loop()
{
    servidor.handleClient();
    delay(2);
}
