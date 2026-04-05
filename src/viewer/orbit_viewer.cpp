/*************************
 * File: orbit_viewer.cpp
 * Author: Sinan Demir
 * Date: 11/27/2025
 * Purpose: OpenGL 3D viewer for Solar System N-body orbits.
 *
 * Notes:
 *  - Uses option C lighting (ambient-boosted Lambert + rim)
 *  - Dynamically renders ALL bodies found in orbit_three_body.csv
 *  - HUD legend (Sun / Earth / Moon) in top-left
 *  - Click legend squares to change camera center (Sun / Earth / Moon)
 *  - Scroll = zoom, RMB drag = orbit
 *  - Keyboard 1–0 = recenter camera on Sun..Neptune
 *  - Distances:
 *      meters → GL via 1 GL = 5e9 m, then uniformly scaled to 2%
 *      (so outer planets are visible in one view)
 *  - Radii:
 *      physically scaled (no extra radius exaggeration)
 *  - Moon:
 *      orbit exaggerated 15× around Earth for visibility
 *************************/

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp> // includes glm::infinitePerspective
#include <glm/gtc/type_ptr.hpp>

#include "viewer/sphere_mesh.h"

// ---------------------------
// Global Viewer State
// ---------------------------
static int g_windowWidth = 1280;
static int g_windowHeight = 720;

// Orbit camera spherical coords
static float g_yaw = glm::radians(45.0f);
static float g_pitch = glm::radians(20.0f);
static float g_radius = 250.0f; // distance from target (in GL units)

static bool g_mouseRotating = false;
static double g_lastMouseX = 0.0;
static double g_lastMouseY = 0.0;

// Camera target selection
enum class CameraTarget
{
    Barycenter = 0,
    Sun,
    Mercury,
    Venus,
    Earth,
    Moon,
    Mars,
    Jupiter,
    Saturn,
    Uranus,
    Neptune
};

static CameraTarget g_cameraTarget = CameraTarget::Barycenter;

// Legend rendering objects (2D)
static GLuint g_legendShader = 0;
static GLuint g_legendVAO = 0;
static GLuint g_legendVBO = 0;
static GLint g_legLocOffset = -1;
static GLint g_legLocScale = -1;
static GLint g_legLocColor = -1;

// Legend layout (in pixels)
static const float LEGEND_BASE_X = 28.0f;  // from left
static const float LEGEND_BASE_Y = 40.0f;  // from top
static const float LEGEND_SPACING = 24.0f; // between rows
static const float LEGEND_SIZE_PX = 14.0f; // base square size

// -------------------------------------------------
// Playback control
// -------------------------------------------------
static double g_lastTime    = 0.0;
static double g_simSpeed    = 1440.0;  // frames per real second
static bool   g_paused      = false;
static double g_accumulator = 0.0;

// --------------------------------------------------
// N-body rendering data
// --------------------------------------------------

struct BodyRenderInfo
{
    std::string name;
    double      mass = 0.0;
    glm::vec3   color;
    float       radius;
    std::vector<glm::vec3> positions;
    SphereMesh  mesh;
    GLuint orbitVAO = 0;
    GLuint orbitVBO = 0;
};

// ── CSV Metadata ─────────────────────────────────────────────────────────────
struct CSVMetadata
{
    int    stride = 1;
    double dt     = 3600.0;
    std::unordered_map<std::string, double> masses; // name → kg
};

static CSVMetadata g_metadata;

static std::vector<BodyRenderInfo> g_bodies;
static std::unordered_map<std::string, size_t> g_bodyIndex;
static size_t g_numFrames = 0;
static size_t g_frameIndex = 0;

// Forward declarations
static void handleLegendClick(double mouseX, double mouseY);
static glm::vec3 getBodyPos(const std::string& name);
static bool initBodiesFromCSV(const std::string& path);
static void buildOrbitBuffers();
static void destroyOrbitBuffers();

// --------------------------------------------------
// Callbacks
// --------------------------------------------------
/**
 * @brief Handles window resize events by updating the viewport.
 */
static void framebuffer_size_callback(GLFWwindow*, int w, int h)
{
    g_windowWidth = w;
    g_windowHeight = h;
    glViewport(0, 0, w, h);
}

/**
 * @brief Handles mouse button events for:
 *  - RMB press/release → start/stop camera rotation
 *  - LMB press on legend → change camera target
 */
static void mouse_button_callback(GLFWwindow* win, int button, int action, int /*mods*/)
{
    if (button == GLFW_MOUSE_BUTTON_RIGHT)
    {
        if (action == GLFW_PRESS)
        {
            g_mouseRotating = true;
            glfwGetCursorPos(win, &g_lastMouseX, &g_lastMouseY);
        }
        else if (action == GLFW_RELEASE)
        {
            g_mouseRotating = false;
        }
    }

    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
    {
        double mx, my;
        glfwGetCursorPos(win, &mx, &my);
        handleLegendClick(mx, my);
    }
}

/**
 * @brief Handles cursor position changes for mouse rotation.
 */
static void cursor_pos_callback(GLFWwindow*, double xpos, double ypos)
{
    if (!g_mouseRotating)
        return;
    double dx = xpos - g_lastMouseX;
    double dy = ypos - g_lastMouseY;

    g_lastMouseX = xpos;
    g_lastMouseY = ypos;

    g_yaw += static_cast<float>(dx) * 0.005f;
    g_pitch -= static_cast<float>(dy) * 0.005f;

    g_pitch = glm::clamp(g_pitch, glm::radians(-89.0f), glm::radians(89.0f));
}

/**
 * @brief Mouse wheel zoom: scroll up = zoom in, scroll down = zoom out.
 *        Zoom speed adapts to current radius so inner / outer system feel
 * usable.
 */
static void scroll_callback(GLFWwindow*, double /*xoff*/, double yoff)
{
    float zoomSpeed = std::max(0.0000005f, g_radius * 0.1f);
    g_radius -= static_cast<float>(yoff) * zoomSpeed;
    g_radius = glm::clamp(g_radius, 0.00000001f, 100000.0f);
}

/**
 * @brief Keyboard controls:
 *  1 = Sun, 2 = Mercury, 3 = Venus, 4 = Earth, 5 = Moon,
 *  6 = Mars, 7 = Jupiter, 8 = Saturn, 9 = Uranus, 0 = Neptune
 */
static void key_callback(GLFWwindow* /*win*/, int key, int /*scancode*/, int action, int /*mods*/)
{
    if (action != GLFW_PRESS)
        return;

    switch (key)
    {
    case GLFW_KEY_1:
        g_cameraTarget = CameraTarget::Sun;
        break;
    case GLFW_KEY_2:
        g_cameraTarget = CameraTarget::Mercury;
        break;
    case GLFW_KEY_3:
        g_cameraTarget = CameraTarget::Venus;
        break;
    case GLFW_KEY_4:
        g_cameraTarget = CameraTarget::Earth;
        break;
    case GLFW_KEY_5:
        g_cameraTarget = CameraTarget::Moon;
        break;
    case GLFW_KEY_6:
        g_cameraTarget = CameraTarget::Mars;
        break;
    case GLFW_KEY_7:
        g_cameraTarget = CameraTarget::Jupiter;
        break;
    case GLFW_KEY_8:
        g_cameraTarget = CameraTarget::Saturn;
        break;
    case GLFW_KEY_9:
        g_cameraTarget = CameraTarget::Uranus;
        break;
    case GLFW_KEY_0:
        g_cameraTarget = CameraTarget::Neptune;
        break;
    default:
        break;
    }
}

// --------------------------------------------------
// Shader helpers
// --------------------------------------------------
/**
 * @brief Compiles a shader of given type from source code.
 */
static GLuint compileShader(GLenum type, const char* src)
{
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);

    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok)
    {
        GLint len = 0;
        glGetShaderiv(s, GL_INFO_LOG_LENGTH, &len);
        std::string log(len, '\0');
        glGetShaderInfoLog(s, len, nullptr, log.data());
        std::cerr << "❌ Shader error:\n" << log << "\n";
    }
    return s;
}

/**
 * @brief Creates an OpenGL program from a vertex + fragment shader pair.
 */
static GLuint createProgram(const char* vsSrc, const char* fsSrc)
{
    GLuint vs = compileShader(GL_VERTEX_SHADER, vsSrc);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fsSrc);

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);

    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok)
    {
        GLint len = 0;
        glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &len);
        std::string log(len, '\0');
        glGetProgramInfoLog(prog, len, nullptr, log.data());
        std::cerr << "❌ Link error:\n" << log << "\n";
    }

    glDeleteShader(vs);
    glDeleteShader(fs);
    return prog;
}

// --------------------------------------------------
// Legend helpers (2D quads in NDC)
// --------------------------------------------------

/**
 * @brief Initializes the legend renderer: shader + unit quad VAO/VBO.
 *
 * Draws small colored squares in NDC that we then place in pixel space.
 */
static void initLegendRenderer()
{
    const char* legendVs = R"GLSL(
        #version 330 core
        layout(location = 0) in vec2 aPos;

        uniform vec2 uOffset;  // NDC center
        uniform vec2 uScale;   // NDC scale

        void main() {
            vec2 pos = aPos * uScale + uOffset;
            gl_Position = vec4(pos, 0.0, 1.0);
        }
    )GLSL";

    const char* legendFs = R"GLSL(
        #version 330 core
        out vec4 FragColor;
        uniform vec3 uColor;

        void main() {
            FragColor = vec4(uColor, 1.0);
        }
    )GLSL";

    g_legendShader = createProgram(legendVs, legendFs);

    g_legLocOffset = glGetUniformLocation(g_legendShader, "uOffset");
    g_legLocScale = glGetUniformLocation(g_legendShader, "uScale");
    g_legLocColor = glGetUniformLocation(g_legendShader, "uColor");

    // Unit square centered at origin, two triangles
    float quadVerts[] = {-0.5f, -0.5f, 0.5f, -0.5f, 0.5f,  0.5f,

                         -0.5f, -0.5f, 0.5f, 0.5f,  -0.5f, 0.5f};

    glGenVertexArrays(1, &g_legendVAO);
    glGenBuffers(1, &g_legendVBO);

    glBindVertexArray(g_legendVAO);
    glBindBuffer(GL_ARRAY_BUFFER, g_legendVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVerts), quadVerts, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

/**
 * @brief Draws a small colored box at a specified pixel position.
 *
 * @param centerPx  X center in window pixels.
 * @param centerPy  Y center in window pixels.
 * @param sizePx    Square size in pixels.
 * @param color     RGB color.
 */
static void drawLegendBox(float centerPx, float centerPy, float sizePx, const glm::vec3& color)
{
    if (g_windowWidth <= 0 || g_windowHeight <= 0)
        return;

    // Convert center in pixels → NDC
    float x_ndc = 2.0f * centerPx / static_cast<float>(g_windowWidth) - 1.0f;
    float y_ndc = 1.0f - 2.0f * centerPy / static_cast<float>(g_windowHeight);

    // Convert size in pixels → NDC scale (quad is [-0.5..0.5])
    float sx = sizePx / static_cast<float>(g_windowWidth) * 2.0f;
    float sy = sizePx / static_cast<float>(g_windowHeight) * 2.0f;

    glUseProgram(g_legendShader);
    glUniform2f(g_legLocOffset, x_ndc, y_ndc);
    glUniform2f(g_legLocScale, sx, sy);
    glUniform3fv(g_legLocColor, 1, glm::value_ptr(color));

    glBindVertexArray(g_legendVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

/**
 * @brief Handle a left-click; if it lands on a legend box,
 *        update camera target (Sun/Earth/Moon).
 */
static void handleLegendClick(double mouseX, double mouseY)
{
    // Legend row centers in pixels
    float centersY[3] = {LEGEND_BASE_Y, LEGEND_BASE_Y + LEGEND_SPACING,
                         LEGEND_BASE_Y + 2.0f * LEGEND_SPACING};

    float halfSize = LEGEND_SIZE_PX * 0.5f;
    float x0 = LEGEND_BASE_X - halfSize;
    float x1 = LEGEND_BASE_X + halfSize;

    for (int i = 0; i < 3; ++i)
    {
        float yCenter = centersY[i];
        float y0 = yCenter - halfSize;
        float y1 = yCenter + halfSize;

        if (mouseX >= x0 && mouseX <= x1 && mouseY >= y0 && mouseY <= y1)
        {

            switch (i)
            {
            case 0:
                g_cameraTarget = CameraTarget::Sun;
                break;
            case 1:
                g_cameraTarget = CameraTarget::Earth;
                break;
            case 2:
                g_cameraTarget = CameraTarget::Moon;
                break;
            }

            std::cout << "📌 Camera target set to "
                      << (i == 0 ? "Sun" : (i == 1 ? "Earth" : "Moon")) << "\n";
            return;
        }
    }
}

// --------------------------------------------------
// Physical scaling & N-body helpers
// --------------------------------------------------

// Distance scale: 1 GL unit = 5e9 meters
static constexpr float DIST_SCALE_METERS = 1.0f / 5e9f;

// Additional uniform compression for visualization.
// After converting meters → GL, we multiply positions by this:
// 1.0f  = no extra compression (true scale in GL)
// 0.02f = 2% of that distance → outer planets pulled in so you can see them
static constexpr float DIST_VIS_SCALE = 0.02f;

// Moon orbit exaggeration (for visibility). Set to 1.0f for strict realism.
static constexpr float MOON_EXAGGERATION = 15.0f;

// Color map for Solar System bodies
static glm::vec3 colorForBody(const std::string& name)
{
    if (name == "Sun")
        return {1.4f, 1.1f, 0.3f};
    if (name == "Mercury")
        return {0.7f, 0.7f, 0.7f};
    if (name == "Venus")
        return {1.0f, 0.9f, 0.6f};
    if (name == "Earth")
        return {0.2f, 0.8f, 1.2f};
    if (name == "Moon")
        return {0.85f, 0.85f, 0.92f};
    if (name == "Mars")
        return {0.9f, 0.3f, 0.2f};
    if (name == "Jupiter")
        return {1.0f, 0.7f, 0.4f};
    if (name == "Saturn")
        return {1.0f, 0.8f, 0.5f};
    if (name == "Uranus")
        return {0.5f, 0.8f, 1.0f};
    if (name == "Neptune")
        return {0.3f, 0.4f, 1.0f};
    return {1.0f, 1.0f, 1.0f};
}

// Physical radii in meters → GL units (no exaggeration)
static float radiusForBody(const std::string& name)
{
    float r_m = 6.0e6f; // default ~Earth-sized as fallback

    if (name == "Sun")
        r_m = 6.9634e8f;
    else if (name == "Mercury")
        r_m = 2.4397e6f;
    else if (name == "Venus")
        r_m = 6.0518e6f;
    else if (name == "Earth")
        r_m = 6.3710e6f;
    else if (name == "Moon")
        r_m = 1.7374e6f;
    else if (name == "Mars")
        r_m = 3.3895e6f;
    else if (name == "Jupiter")
        r_m = 6.9911e7f;
    else if (name == "Saturn")
        r_m = 5.8232e7f;
    else if (name == "Uranus")
        r_m = 2.5362e7f;
    else if (name == "Neptune")
        r_m = 2.4622e7f;

    // meters → GL units (no extra radius exaggeration)
    return r_m * DIST_SCALE_METERS;
}

/**
 * @brief Returns the current position of a body (in GL units) for the active
 * frame. If body is missing or has no data, returns (0,0,0).
 */
static glm::vec3 getBodyPos(const std::string& name)
{
    auto it = g_bodyIndex.find(name);
    if (it == g_bodyIndex.end())
        return glm::vec3(0.0f);
    size_t idx = it->second;
    if (g_bodies.empty() || g_bodies[idx].positions.empty())
        return glm::vec3(0.0f);

    size_t frame = (g_numFrames == 0) ? 0 : (g_frameIndex % g_numFrames);
    return g_bodies[idx].positions[frame];
}

// ── parseMetadataComment ─────────────────────────────────────────────────────
/**
 * @brief Parses the metadata comment line from a simulation CSV.
 *
 * Expected format:
 *   # stride=1 dt=60 bodies=Sun:1.989e30,Earth:5.972e24,Moon:7.342e22
 *
 * All fields are optional — if missing, defaults in CSVMetadata are kept.
 *
 * @param line  The raw comment line (including the leading #)
 * @param meta  Output metadata struct to fill
 */
static void parseMetadataComment(const std::string& line, CSVMetadata& meta)
{
    // ── stride ───────────────────────────────────────────────────────────────
    auto stridePos = line.find("stride=");
    if (stridePos != std::string::npos)
    {
        try { meta.stride = std::stoi(line.substr(stridePos + 7)); }
        catch (...) {}
    }

    // ── dt ───────────────────────────────────────────────────────────────────
    auto dtPos = line.find("dt=");
    if (dtPos != std::string::npos)
    {
        try { meta.dt = std::stod(line.substr(dtPos + 3)); }
        catch (...) {}
    }

    // ── bodies=Sun:1.989e30,Earth:5.972e24,... ───────────────────────────────
    auto bodiesPos = line.find("bodies=");
    if (bodiesPos == std::string::npos)
        return;

    std::string bodiesStr = line.substr(bodiesPos + 7);

    // Split on commas → each token is "Name:mass"
    std::stringstream ss(bodiesStr);
    std::string token;
    while (std::getline(ss, token, ','))
    {
        // Trim whitespace
        token.erase(0, token.find_first_not_of(" \t\r\n"));
        token.erase(token.find_last_not_of(" \t\r\n") + 1);

        auto colonPos = token.find(':');
        if (colonPos == std::string::npos)
            continue;

        std::string name    = token.substr(0, colonPos);
        std::string massStr = token.substr(colonPos + 1);

        try { meta.masses[name] = std::stod(massStr); }
        catch (...) {}
    }
} // end parseMetadataComment

/**
 * @brief Initialize N-body data by reading orbit_three_body.csv.
 *        - Detects all x_, y_, z_ position columns
 *        - Scales meters to GL units
 *        - Compresses distances (2%) for visibility
 *        - Optionally exaggerates Moon orbit for visibility
 */
static bool initBodiesFromCSV(const std::string& path)
{
    std::ifstream file(path);
    if (!file.is_open())
    {
        std::cerr << "❌ Could not open CSV: " << path << "\n";
        return false;
    }

    std::string line;
    if (!std::getline(file, line))
    {
        std::cerr << "⚠️ Empty CSV: " << path << "\n";
        return false;
    }

    // ── Check for metadata comment ────────────────────────────────────────────
    if (line.rfind("# ", 0) == 0)
    {
        parseMetadataComment(line, g_metadata);

        std::cout << "📊 Metadata:"
                  << "  stride=" << g_metadata.stride
                  << "  dt="     << g_metadata.dt     << "s"
                  << "  bodies=" << g_metadata.masses.size()
                  << "\n";

        for (const auto& [name, mass] : g_metadata.masses)
            std::cout << "     " << name << " → " << mass << " kg\n";

        // Advance to the actual column header line
        if (!std::getline(file, line))
        {
            std::cerr << "⚠️ CSV has metadata but no column header\n";
            return false;
        }
    }
    // If no metadata comment, line already contains the column header so continue
    
    std::vector<std::string> columns;
    std::string col;
    std::stringstream ss(line);
    while (std::getline(ss, col, ','))
    {
        columns.push_back(col);
    }

    // Identify body columns: x_name,y_name,z_name groups.
    struct Triplet
    {
        int ix, iy, iz;
    };
    std::vector<Triplet> bodyCols;

    for (size_t i = 0; i + 2 < columns.size(); ++i)
    {
        if (columns[i].rfind("x_", 0) == 0)
        {
            std::string name = columns[i].substr(2);
            int ix = static_cast<int>(i);
            int iy = static_cast<int>(i + 1);
            int iz = static_cast<int>(i + 2);

            BodyRenderInfo body;
            body.name  = name;
            body.mass  = 0.0;   // will be set from metadata if available

            // Look up mass from parsed metadata
            auto massIt = g_metadata.masses.find(name);
            if (massIt != g_metadata.masses.end())
            {
                body.mass = massIt->second;
            }
            else
            {
                std::cerr << "⚠️  No mass data for " << name
                          << " — visual radius will use fallback\n";
            }

            body.color  = colorForBody(name);   // still hardcoded for now — Phase 3 fixes this
            body.radius = radiusForBody(name);  // still hardcoded for now — Phase 2 fixes this
            g_bodyIndex[name] = g_bodies.size();
            g_bodies.push_back(std::move(body));
            bodyCols.push_back({ix, iy, iz});
        }
    }

    if (g_bodies.empty())
    {
        std::cerr << "⚠️ No x_* body columns found in CSV header.\n";
        return false;
    }

    constexpr float SCALE_METERS = DIST_SCALE_METERS;

    size_t lineCount = 0;
    while (std::getline(file, line))
    {
        if (line.empty())
            continue;
        std::stringstream ss(line);
        std::vector<double> rowValues;
        std::string v;
        while (std::getline(ss, v, ','))
        {
            try
            {
                rowValues.push_back(std::stod(v));
            }
            catch (...)
            {
                rowValues.push_back(0.0);
            }
        }

        if (rowValues.size() < columns.size())
        {
            // malformed row, skip
            continue;
        }

        // Per-frame positions for all bodies (scaled).
        std::vector<glm::vec3> framePos(g_bodies.size(), glm::vec3(0.0f));

        // First, load raw scaled positions.
        for (size_t bi = 0; bi < g_bodies.size(); ++bi)
        {
            const auto& trips = bodyCols[bi];
            double x = rowValues[trips.ix];
            double y = rowValues[trips.iy];
            double z = rowValues[trips.iz];

            glm::vec3 p(static_cast<float>(x * SCALE_METERS), static_cast<float>(y * SCALE_METERS),
                        static_cast<float>(z * SCALE_METERS));

            // ----------------------------------------
            // ✨ VISUAL DISTANCE COMPRESSION (2%)
            // ----------------------------------------
            // This keeps all orbital shapes and relative geometry,
            // but pulls the whole system closer to the camera so
            // Jupiter / Saturn / Uranus / Neptune are actually visible.
            p *= DIST_VIS_SCALE;

            framePos[bi] = p;
        }

        // Exaggerate Moon orbit if both Earth and Moon exist.
        if (MOON_EXAGGERATION != 1.0f)
        {
            auto itEarth = g_bodyIndex.find("Earth");
            auto itMoon = g_bodyIndex.find("Moon");
            if (itEarth != g_bodyIndex.end() && itMoon != g_bodyIndex.end())
            {
                size_t eIdx = itEarth->second;
                size_t mIdx = itMoon->second;
                glm::vec3 earthPos = framePos[eIdx];
                glm::vec3 moonPos = framePos[mIdx];
                glm::vec3 offset = moonPos - earthPos;
                framePos[mIdx] = earthPos + offset * MOON_EXAGGERATION;
            }
        }

        // Append frame positions to each body.
        for (size_t bi = 0; bi < g_bodies.size(); ++bi)
        {
            g_bodies[bi].positions.push_back(framePos[bi]);
        }

        ++lineCount;
    }

    g_numFrames = lineCount;
    std::cout << "📄 Loaded " << g_numFrames << " frames for " << g_bodies.size() << " bodies from "
              << path << "\n";
    return (g_numFrames > 0);
}

static void buildOrbitBuffers()
{
    for (auto& body : g_bodies)
    {
        if (body.positions.empty())
            continue;

        glGenVertexArrays(1, &body.orbitVAO);
        glGenBuffers(1, &body.orbitVBO);

        glBindVertexArray(body.orbitVAO);
        glBindBuffer(GL_ARRAY_BUFFER, body.orbitVBO);

        glBufferData(GL_ARRAY_BUFFER, body.positions.size() * sizeof(glm::vec3),
                     body.positions.data(), GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);

        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
}

static void destroyOrbitBuffers()
{
    for (auto& body : g_bodies)
    {
        if (body.orbitVBO != 0)
            glDeleteBuffers(1, &body.orbitVBO);
        if (body.orbitVAO != 0)
            glDeleteVertexArrays(1, &body.orbitVAO);

        body.orbitVBO = 0;
        body.orbitVAO = 0;
    }
}

// --------------------------------------------------
// MAIN
// --------------------------------------------------
/**
 * @brief Entry point for the Orbit Viewer application.
 */
int main(int argc, char** argv)
{
    // Init N-body positions first (Solar System) from simulation output
    std::string csvPath = (argc > 1) ? argv[1] : "./build/orbit_three_body.csv";
    if (!initBodiesFromCSV(csvPath))
    {
        return -1;
    }

    // ── Set default playback speed from metadata ──────────────────────────────
    // Target: 1 simulated day per real second
    // simSecondsPerFrame = stride × dt
    // framesPerSimDay    = 86400 / simSecondsPerFrame
    {
        double simSecondsPerFrame = static_cast<double>(g_metadata.stride) * g_metadata.dt;
        double framesPerSimDay    = 86400.0 / simSecondsPerFrame;

        // Clamp to a sane range — don't play at 0.001 fps or 100000 fps
        g_simSpeed = glm::clamp(framesPerSimDay, 1.0, 10000.0);

        std::cout << "⏱  Playback: 1 sim-day per real-second"
                  << "  (" << g_simSpeed << " frames/s)\n";
    }

    // ----------------- GLFW init -----------------
    if (!glfwInit())
    {
        std::cerr << "❌ Failed to init GLFW\n";
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* win = glfwCreateWindow(g_windowWidth, g_windowHeight,
                                       ("Orbit Viewer — " + csvPath).c_str(), nullptr, nullptr);

    if (!win)
    {
        std::cerr << "❌ Failed to create window\n";
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(win);
    glfwSetFramebufferSizeCallback(win, framebuffer_size_callback);
    glfwSetMouseButtonCallback(win, mouse_button_callback);
    glfwSetCursorPosCallback(win, cursor_pos_callback);
    glfwSetScrollCallback(win, scroll_callback);
    glfwSetKeyCallback(win, key_callback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cerr << "❌ Failed to init GLAD\n";
        glfwDestroyWindow(win);
        glfwTerminate();
        return -1;
    }

    glEnable(GL_DEPTH_TEST);

    // Build sphere meshes for all bodies now that we have a valid GL context
    for (auto& body : g_bodies)
    {
        body.mesh.build(body.radius, 32, 32);
    }

    // ----------------------------------------------------
    // Create main 3D shader (Option C lighting)
    // ----------------------------------------------------
    const char* vsSrc = R"GLSL(
        #version 330 core
        layout(location = 0) in vec3 aPos;
        layout(location = 1) in vec3 aNormal;

        uniform mat4 uMVP;
        uniform mat4 uModel;

        out vec3 vNormal;
        out vec3 vWorldPos;

        void main() {
            mat3 normalMat = mat3(transpose(inverse(uModel)));
            vNormal   = normalMat * aNormal;
            vWorldPos = vec3(uModel * vec4(aPos, 1.0));
            gl_Position = uMVP * vec4(aPos, 1.0);
        }
    )GLSL";

    const char* fsSrc = R"GLSL(
        #version 330 core

        in vec3 vNormal;
        in vec3 vWorldPos;

        out vec4 FragColor;

        uniform vec3 uColor;
        uniform vec3 uLightPos;
        uniform vec3 uViewPos;

        void main() {
            vec3 N = normalize(vNormal);
            vec3 L = normalize(uLightPos - vWorldPos);
            vec3 V = normalize(uViewPos - vWorldPos);
            vec3 H = normalize(L + V);

            // Lambert + Blinn–Phong
            float diff = max(dot(N, L), 0.0);
            float spec = pow(max(dot(N, H), 0.0), 32.0);
            float ambient = 0.18;

            vec3 base = uColor * (ambient + diff)
                      + vec3(0.4) * spec;

            // Rim light for cinematic look
            float rim = pow(1.0 - max(dot(N, V), 0.0), 2.0);
            vec3 rimColor = vec3(0.3, 0.4, 0.9) * rim * 0.5;

            vec3 color = base + rimColor;

            // Gamma
            color = pow(color, vec3(1.0 / 2.2));

            FragColor = vec4(color, 1.0);
        }
    )GLSL";

    const char* orbitVs = R"GLSL(
        #version 330 core
        layout(location = 0) in vec3 aPos;

        uniform mat4 uVP;

        void main()
        {
            gl_Position = uVP * vec4(aPos, 1.0);
        }
    )GLSL";

    const char* orbitFs = R"GLSL(
        #version 330 core
        out vec4 FragColor;

        uniform vec3 uColor;

        void main()
        {
            FragColor = vec4(uColor, 1.0);
        }
    )GLSL";

    GLuint orbitShader = createProgram(orbitVs, orbitFs);
    GLint orbitLocVP = glGetUniformLocation(orbitShader, "uVP");
    GLint orbitLocColor = glGetUniformLocation(orbitShader, "uColor");

    GLuint shader = createProgram(vsSrc, fsSrc);
    GLint locMVP = glGetUniformLocation(shader, "uMVP");
    GLint locModel = glGetUniformLocation(shader, "uModel");
    GLint locColor = glGetUniformLocation(shader, "uColor");
    GLint locLight = glGetUniformLocation(shader, "uLightPos");
    GLint locViewPos = glGetUniformLocation(shader, "uViewPos");

    // ----------------------------------------------------
    // Init legend renderer (2D colored boxes in NDC)
    // ----------------------------------------------------
    initLegendRenderer();

    buildOrbitBuffers();

    // ----------------------------------------------------
    // Main render loop
    // ----------------------------------------------------
    while (!glfwWindowShouldClose(win))
    {
        glfwPollEvents();

        if (g_numFrames > 0)
        {
            g_frameIndex = (g_frameIndex + 1) % g_numFrames;
        }

        glClearColor(0.02f, 0.02f, 0.05f, 1.0f); // deep navy space
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        if (!g_bodies.empty() && g_numFrames > 0)
        {
            // Determine camera target position
            glm::vec3 target(0.0f);
            switch (g_cameraTarget)
            {
            case CameraTarget::Sun:
                target = getBodyPos("Sun");
                break;
            case CameraTarget::Mercury:
                target = getBodyPos("Mercury");
                break;
            case CameraTarget::Venus:
                target = getBodyPos("Venus");
                break;
            case CameraTarget::Earth:
                target = getBodyPos("Earth");
                break;
            case CameraTarget::Moon:
                target = getBodyPos("Moon");
                break;
            case CameraTarget::Mars:
                target = getBodyPos("Mars");
                break;
            case CameraTarget::Jupiter:
                target = getBodyPos("Jupiter");
                break;
            case CameraTarget::Saturn:
                target = getBodyPos("Saturn");
                break;
            case CameraTarget::Uranus:
                target = getBodyPos("Uranus");
                break;
            case CameraTarget::Neptune:
                target = getBodyPos("Neptune");
                break;
            case CameraTarget::Barycenter:
            default:
                target = glm::vec3(0.0f);
                break;
            }

            // Camera offset in local spherical coords
            glm::vec3 camOffset(g_radius * cos(g_pitch) * sin(g_yaw), g_radius * sin(g_pitch),
                                g_radius * cos(g_pitch) * cos(g_yaw));

            glm::vec3 camPos = target + camOffset;

            float aspect = float(g_windowWidth) / float(g_windowHeight);

            // Reverse-Z style infinite perspective: tiny near plane, infinite far.
            glm::mat4 proj = glm::infinitePerspective(glm::radians(45.0f), aspect,
                                                      0.000001f // near plane in GL units
            );

            glm::mat4 view = glm::lookAt(camPos, target, glm::vec3(0, 1, 0));

            glm::mat4 vp = proj * view;
            size_t frame = g_frameIndex % g_numFrames;

            // ---------------- Orbit line draw ----------------
            glEnable(GL_DEPTH_TEST);
            glUseProgram(orbitShader);
            glUniformMatrix4fv(orbitLocVP, 1, GL_FALSE, glm::value_ptr(vp));
            glLineWidth(2.0f);

            for (auto& body : g_bodies)
            {
                if (body.orbitVAO == 0 || body.positions.empty())
                    continue;

                GLsizei count = static_cast<GLsizei>(std::min(frame + 1, body.positions.size()));
                if (count < 2)
                    continue;

                glUniform3fv(orbitLocColor, 1, glm::value_ptr(body.color));
                glBindVertexArray(body.orbitVAO);
                glDrawArrays(GL_LINE_STRIP, 0, count);
            }

            glDisable(GL_DEPTH_TEST);

            glBindVertexArray(0);

            // ---------------- Sphere draw ----------------
            glUseProgram(shader);

            // view position for specular
            glUniform3fv(locViewPos, 1, glm::value_ptr(camPos));

            // light at Sun position (or origin if missing)
            glm::vec3 sunPos = getBodyPos("Sun");
            glUniform3fv(locLight, 1, glm::value_ptr(sunPos));

            for (auto& body : g_bodies)
            {
                if (frame >= body.positions.size())
                    continue;

                glm::mat4 model = glm::translate(glm::mat4(1.0f), body.positions[frame]);
                glm::mat4 mvp = proj * view * model;

                glUniformMatrix4fv(locMVP, 1, GL_FALSE, glm::value_ptr(mvp));
                glUniformMatrix4fv(locModel, 1, GL_FALSE, glm::value_ptr(model));
                glUniform3fv(locColor, 1, glm::value_ptr(body.color));

                body.mesh.draw();
            }
        }

        // ------------------------------------------------
        // HUD legend (Sun / Earth / Moon) – top-left
        // ------------------------------------------------
        glDisable(GL_DEPTH_TEST);

        float size = LEGEND_SIZE_PX;
        float sizeSel = LEGEND_SIZE_PX * 1.4f; // highlight selected

        float y0 = LEGEND_BASE_Y;
        float y1 = LEGEND_BASE_Y + LEGEND_SPACING;
        float y2 = LEGEND_BASE_Y + 2.0f * LEGEND_SPACING;

        // Sun icon (top)
        drawLegendBox(LEGEND_BASE_X, y0, (g_cameraTarget == CameraTarget::Sun ? sizeSel : size),
                      glm::vec3(1.4f, 1.1f, 0.3f));

        // Earth icon (middle)
        drawLegendBox(LEGEND_BASE_X, y1, (g_cameraTarget == CameraTarget::Earth ? sizeSel : size),
                      glm::vec3(0.2f, 0.8f, 1.2f));

        // Moon icon (bottom)
        drawLegendBox(LEGEND_BASE_X, y2, (g_cameraTarget == CameraTarget::Moon ? sizeSel : size),
                      glm::vec3(0.85f, 0.85f, 0.92f));

        glEnable(GL_DEPTH_TEST);

        glfwSwapBuffers(win);
    }

    glfwDestroyWindow(win);

    // cleanup legend objects
    destroyOrbitBuffers();
    glDeleteProgram(orbitShader);
    glDeleteBuffers(1, &g_legendVBO);
    glDeleteVertexArrays(1, &g_legendVAO);
    glDeleteProgram(g_legendShader);

    glfwTerminate();
    return 0;
}
