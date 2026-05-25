#include <iostream>
#include <vector>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// =====================================================================
//  CONFIGURACION DE CAMARA
// =====================================================================
struct Camara {
    glm::vec3 pos = glm::vec3(0.f, 0.f, 8.f);
    glm::vec3 frente = glm::vec3(0.f, 0.f, -1.f);
    glm::vec3 arriba = glm::vec3(0.f, 1.f, 0.f);
    float yaw = -90.f;
    float pitch = 0.f;
    float ultX = 0.f;
    float ultY = 0.f;
    bool primerMov = true;
};

Camara cam;

// =====================================================================
//  SHADERS
// =====================================================================

// --- Shader del cubo (textura + color azul) ---
const char* srcVertCubo =
"#version 330 core\n"
"layout(location=0) in vec3 posicion;\n"
"layout(location=1) in vec2 uv;\n"
"out vec2 coordTex;\n"
"uniform mat4 mvp;\n"
"void main(){\n"
"    gl_Position = mvp * vec4(posicion, 1.0);\n"
"    coordTex = uv;\n"
"}\0";

const char* srcFragCubo =
"#version 330 core\n"
"in vec2 coordTex;\n"
"out vec4 colorFinal;\n"
"uniform sampler2D tex;\n"
"void main(){\n"
"    vec4 colorTex = texture(tex, coordTex);\n"
"    vec4 azul = vec4(0.1, 0.3, 1.0, 1.0);\n"
"    colorFinal = mix(colorTex, azul, 0.6);\n"
"}\0";

// --- Shader de las líneas Bézier (color sólido) ---
const char* srcVertLinea =
"#version 330 core\n"
"layout(location=0) in vec3 posicion;\n"
"uniform mat4 vp;\n"
"void main(){\n"
"    gl_Position = vp * vec4(posicion, 1.0);\n"
"}\0";

const char* srcFragLinea =
"#version 330 core\n"
"out vec4 colorFinal;\n"
"uniform vec3 colorLinea;\n"
"void main(){\n"
"    colorFinal = vec4(colorLinea, 1.0);\n"
"}\0";

// =====================================================================
//  CALLBACKS
// =====================================================================
void alRedimensionar(GLFWwindow* win, int w, int h) {
    glViewport(0, 0, w, h);
}

void alMoverMouse(GLFWwindow* win, double x, double y) {
    if (cam.primerMov) {
        cam.ultX = (float)x;
        cam.ultY = (float)y;
        cam.primerMov = false;
    }

    float dx = (float)x - cam.ultX;
    float dy = cam.ultY - (float)y;
    cam.ultX = (float)x;
    cam.ultY = (float)y;

    const float sens = 0.1f;
    cam.yaw += dx * sens;
    cam.pitch += dy * sens;
    cam.pitch = glm::clamp(cam.pitch, -89.f, 89.f);

    glm::vec3 dir;
    dir.x = cos(glm::radians(cam.yaw)) * cos(glm::radians(cam.pitch));
    dir.y = sin(glm::radians(cam.pitch));
    dir.z = sin(glm::radians(cam.yaw)) * cos(glm::radians(cam.pitch));
    cam.frente = glm::normalize(dir);
}

// =====================================================================
//  UTILIDADES
// =====================================================================
GLuint compilarPrograma(const char* vert, const char* frag) {
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vert, NULL);
    glCompileShader(vs);

    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &frag, NULL);
    glCompileShader(fs);

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);

    glDeleteShader(vs);
    glDeleteShader(fs);
    return prog;
}

// Evalúa la curva Bézier cúbica en t ∈ [0,1]
glm::vec3 evaluarBezier(glm::vec3 A, glm::vec3 B, glm::vec3 C, glm::vec3 D, float t) {
    float u = 1.f - t;
    return u * u * u * A + 3 * u * u * t * B + 3 * u * t * t * C + t * t * t * D;
}

// Genera N+1 puntos a lo largo de la curva para dibujarla como línea
std::vector<glm::vec3> generarPuntosCurva(
    glm::vec3 A, glm::vec3 B, glm::vec3 C, glm::vec3 D, int segmentos)
{
    std::vector<glm::vec3> pts;
    for (int i = 0; i <= segmentos; i++) {
        float t = (float)i / segmentos;
        pts.push_back(evaluarBezier(A, B, C, D, t));
    }
    return pts;
}

// Crea VAO+VBO para una lista de puntos 3D (para GL_LINE_STRIP)
void crearVAOLinea(GLuint& vao, GLuint& vbo, const std::vector<glm::vec3>& pts) {
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, pts.size() * sizeof(glm::vec3), pts.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}

// =====================================================================
//  MAIN
// =====================================================================
int main() {

    // --- Inicializar GLFW ---
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* modo = glfwGetVideoMode(monitor);
    GLFWwindow* ventana = glfwCreateWindow(
        modo->width, modo->height, "Curvas Bezier - OpenGL", monitor, NULL);

    if (!ventana) {
        std::cout << "Error al crear la ventana\n";
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(ventana);
    glfwSetFramebufferSizeCallback(ventana, alRedimensionar);
    glfwSetCursorPosCallback(ventana, alMoverMouse);
    glfwSetInputMode(ventana, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    gladLoadGL();
    glEnable(GL_DEPTH_TEST);

    // ---- Programas de shader ----
    GLuint progCubo = compilarPrograma(srcVertCubo, srcFragCubo);
    GLuint progLinea = compilarPrograma(srcVertLinea, srcFragLinea);

    // =====================================================================
    //  GEOMETRIA DE LA ESTRELLA 3D (5 puntas extrudida)
    //  - 10 vértices en el anillo (5 puntas + 5 valles) x 2 caras (front/back)
    //  - 1 centro front + 1 centro back
    //  - Radio exterior (punta): 0.5   Radio interior (valle): 0.22
    //  - Profundidad Z: +0.15 front / -0.15 back
    // =====================================================================
    struct Vertice { float pos[3]; float uv[2]; };

    const int   NPUNTAS = 5;
    const float R_EXT = 0.5f;
    const float R_INT = 0.22f;
    const float Z_F = 0.15f;
    const float Z_B = -0.15f;
    const float PI = 3.14159265f;

    // Genera los 10 puntos del perfil de estrella en Z dado
    auto anillo = [&](float z) -> std::vector<Vertice> {
        std::vector<Vertice> v;
        for (int i = 0; i < NPUNTAS * 2; i++) {
            float ang = (PI / NPUNTAS) * i - PI / 2.f;
            float r = (i % 2 == 0) ? R_EXT : R_INT;
            float x = r * cos(ang);
            float y = r * sin(ang);
            float u = (x / R_EXT) * 0.5f + 0.5f;
            float vv = (y / R_EXT) * 0.5f + 0.5f;
            v.push_back({ {x, y, z}, {u, vv} });
        }
        return v;
        };

    // Construir todos los vértices:
    // [0..9]  anillo front   (Z = +0.15)
    // [10..19] anillo back   (Z = -0.15)
    // [20]   centro front
    // [21]   centro back
    std::vector<Vertice> estVerts;
    auto af = anillo(Z_F);
    auto ab = anillo(Z_B);
    for (auto& v : af) estVerts.push_back(v);
    for (auto& v : ab) estVerts.push_back(v);
    estVerts.push_back({ {0.f, 0.f, Z_F}, {0.5f, 0.5f} }); // 20 centro front
    estVerts.push_back({ {0.f, 0.f, Z_B}, {0.5f, 0.5f} }); // 21 centro back

    // Índices:
    std::vector<GLuint> estIdx;
    int N = NPUNTAS * 2; // 10

    // Cara frontal: abanico desde centro (20) hacia cada par del anillo front
    for (int i = 0; i < N; i++) {
        estIdx.push_back(20);
        estIdx.push_back(i);
        estIdx.push_back((i + 1) % N);
    }
    // Cara trasera: abanico desde centro (21) — orden invertido para normal correcta
    for (int i = 0; i < N; i++) {
        estIdx.push_back(21);
        estIdx.push_back(10 + (i + 1) % N);
        estIdx.push_back(10 + i);
    }
    // Lados: quad entre anillo front[i]→front[i+1] y back[i]→back[i+1]
    for (int i = 0; i < N; i++) {
        int a = i;
        int b = (i + 1) % N;
        int a2 = 10 + i;
        int b2 = 10 + (i + 1) % N;
        estIdx.push_back(a);  estIdx.push_back(b);  estIdx.push_back(a2);
        estIdx.push_back(b);  estIdx.push_back(b2); estIdx.push_back(a2);
    }

    GLuint vaoCubo, vboCubo, eboCubo;
    glGenVertexArrays(1, &vaoCubo);
    glGenBuffers(1, &vboCubo);
    glGenBuffers(1, &eboCubo);
    glBindVertexArray(vaoCubo);
    glBindBuffer(GL_ARRAY_BUFFER, vboCubo);
    glBufferData(GL_ARRAY_BUFFER, estVerts.size() * sizeof(Vertice), estVerts.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, eboCubo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, estIdx.size() * sizeof(GLuint), estIdx.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertice), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertice), (void*)(offsetof(Vertice, uv)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    GLsizei estIdxCount = (GLsizei)estIdx.size();

    // =====================================================================
    //  TEXTURA
    // =====================================================================
    GLuint textura;
    glGenTextures(1, &textura);
    glBindTexture(GL_TEXTURE_2D, textura);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    int ancho, alto, canales;
    unsigned char* imgData = stbi_load(
        "C:\\Users\\PERSONAL\\Documents\\stb-master\\data\\textura.jpg",
        &ancho, &alto, &canales, 0);
    if (imgData) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, ancho, alto, 0, GL_RGB, GL_UNSIGNED_BYTE, imgData);
        glGenerateMipmap(GL_TEXTURE_2D);
    }
    else {
        std::cout << "No se pudo cargar la textura\n";
    }
    stbi_image_free(imgData);

    // =====================================================================
    //  CURVA 1 — horizontal con ondulacion vertical  (cubo viaja por ella)
    //  Color: AMARILLO
    // =====================================================================
    glm::vec3 c1P0(-4.f, -3.f, -5.f);
    glm::vec3 c1P1(5.f, 4.f, -1.f);
    glm::vec3 c1P2(-5.f, 1.f, 3.f);
    glm::vec3 c1P3(3.f, -3.f, 5.f);

    auto ptosCurva1 = generarPuntosCurva(c1P0, c1P1, c1P2, c1P3, 200);
    GLuint vaoLinea1, vboLinea1;
    crearVAOLinea(vaoLinea1, vboLinea1, ptosCurva1);



    // =====================================================================
    //  ESTADO DE ANIMACION
    // =====================================================================
    float tBezier = 0.f;
    bool  moverObjeto = false;
    bool  espacioAnterior = false;
    bool  camaraEnCurva = false;   // K activa/desactiva cámara sobre la curva
    bool  kAnterior = false;

    // =====================================================================
    //  LOOP PRINCIPAL
    // =====================================================================
    while (!glfwWindowShouldClose(ventana)) {

        // --- Entrada de teclado ---
        const float vel = 0.02f;
        glm::vec3 lateral = glm::normalize(glm::cross(cam.frente, cam.arriba));

        if (glfwGetKey(ventana, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(ventana, true);

        // WASD solo funcionan en modo libre
        if (!camaraEnCurva) {
            if (glfwGetKey(ventana, GLFW_KEY_W) == GLFW_PRESS) cam.pos += vel * cam.frente;
            if (glfwGetKey(ventana, GLFW_KEY_S) == GLFW_PRESS) cam.pos -= vel * cam.frente;
            if (glfwGetKey(ventana, GLFW_KEY_A) == GLFW_PRESS) cam.pos -= lateral * vel;
            if (glfwGetKey(ventana, GLFW_KEY_D) == GLFW_PRESS) cam.pos += lateral * vel;
        }

        // ESPACIO — activa/desactiva movimiento del objeto
        bool espacioAhora = glfwGetKey(ventana, GLFW_KEY_SPACE) == GLFW_PRESS;
        if (espacioAhora && !espacioAnterior) moverObjeto = !moverObjeto;
        espacioAnterior = espacioAhora;

        // K — activa/desactiva cámara sobre la curva
        bool kAhora = glfwGetKey(ventana, GLFW_KEY_K) == GLFW_PRESS;
        if (kAhora && !kAnterior) {
            camaraEnCurva = !camaraEnCurva;
            // Al entrar en modo curva, forzar movimiento activo
            if (camaraEnCurva) moverObjeto = true;
        }
        kAnterior = kAhora;

        // --- Avanzar t sobre la curva ---
        if (moverObjeto) {
            tBezier += 0.00008f;
            if (tBezier > 1.f) tBezier = 0.f;
        }

        // --- Posición actual del cubo en la curva ---
        glm::vec3 posCubo = evaluarBezier(c1P0, c1P1, c1P2, c1P3, tBezier);

        // --- Si cámara en curva: posicionar cámara sobre el cubo ---
        // La dirección de avance se calcula como tangente (t+epsilon)
        if (camaraEnCurva) {
            float tNext = glm::min(tBezier + 0.01f, 1.f);
            glm::vec3 posNext = evaluarBezier(c1P0, c1P1, c1P2, c1P3, tNext);
            glm::vec3 tangente = glm::normalize(posNext - posCubo);

            // Cámara ligeramente por encima del cubo, mirando hacia adelante
            cam.pos = posCubo + glm::vec3(0.f, 0.5f, 0.f);
            cam.frente = tangente;
        }

        // --- Limpiar pantalla ---
        glClearColor(0.08f, 0.08f, 0.12f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // --- Matrices de vista y proyección ---
        int w, h;
        glfwGetFramebufferSize(ventana, &w, &h);
        glm::mat4 proj = glm::perspective(glm::radians(60.f), (float)w / h, 0.1f, 100.f);
        glm::mat4 view = glm::lookAt(cam.pos, cam.pos + cam.frente, cam.arriba);
        glm::mat4 vp = proj * view;

        // ---- Dibujar CURVA 1 (amarillo) ----
        glUseProgram(progLinea);
        glUniformMatrix4fv(glGetUniformLocation(progLinea, "vp"), 1, GL_FALSE, glm::value_ptr(vp));
        glUniform3f(glGetUniformLocation(progLinea, "colorLinea"), 1.0f, 0.9f, 0.1f);
        glBindVertexArray(vaoLinea1);
        glLineWidth(2.5f);
        glDrawArrays(GL_LINE_STRIP, 0, (GLsizei)ptosCurva1.size());

        // ---- Dibujar CUBO (solo visible en modo libre) ----
        if (!camaraEnCurva) {
            glm::mat4 modelo = glm::translate(glm::mat4(1.f), posCubo);
            modelo = glm::rotate(modelo, (float)glfwGetTime(), glm::vec3(1.f, 1.f, 0.f));
            glm::mat4 mvp = vp * modelo;

            glUseProgram(progCubo);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, textura);
            glUniform1i(glGetUniformLocation(progCubo, "tex"), 0);
            glUniformMatrix4fv(glGetUniformLocation(progCubo, "mvp"), 1, GL_FALSE, glm::value_ptr(mvp));
            glBindVertexArray(vaoCubo);
            glDrawElements(GL_TRIANGLES, estIdxCount, GL_UNSIGNED_INT, 0);
        }

        glfwSwapBuffers(ventana);
        glfwPollEvents();
    }

    // --- Liberar recursos ---
    glDeleteVertexArrays(1, &vaoCubo);
    glDeleteBuffers(1, &vboCubo);
    glDeleteBuffers(1, &eboCubo);
    glDeleteVertexArrays(1, &vaoLinea1);
    glDeleteBuffers(1, &vboLinea1);
    glDeleteTextures(1, &textura);
    glDeleteProgram(progCubo);
    glDeleteProgram(progLinea);

    glfwDestroyWindow(ventana);
    glfwTerminate();
    return 0;
}