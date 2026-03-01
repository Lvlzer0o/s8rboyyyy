#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <GL/gl.h>
#include <GL/glu.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <deque>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "player.hpp"
#include "world.hpp"

namespace
{

constexpr float PI = 3.14159265358979323846f;
constexpr float TAU = PI * 2.0f;
constexpr float DEG = 180.0f / PI;

constexpr float WORLD_TIME = 180.0f;
constexpr float GRIND_ENTRY_BONUS = 150;
constexpr float GRIND_EXIT_BONUS = 85;
constexpr float GRIND_SCORE_RATE = 140.0f;
constexpr float GRIND_SPEED_MULT = 1.012f;
constexpr int SEGMENT_COUNT = 12;
constexpr int GROUND_SAMPLES = 72;

constexpr float SKY_R1 = 0.08f;
constexpr float SKY_G1 = 0.16f;
constexpr float SKY_B1 = 0.30f;
constexpr float SKY_R2 = 0.16f;
constexpr float SKY_G2 = 0.35f;
constexpr float SKY_B2 = 0.62f;
constexpr float HORIZON_GLOW = 0.18f;

constexpr bool kUseProceduralSkateboard = false;
constexpr float kProceduralDeckWidth = 0.88f;
constexpr float kProceduralDeckNoseWidth = 0.66f;
constexpr float kProceduralDeckLength = 2.48f;
constexpr float kProceduralTruckY = -0.12f;
constexpr float kProceduralWheelX = 0.40f;
constexpr float kProceduralWheelY = -0.24f;

constexpr float kProceduralTruckRadius = 0.12f;
constexpr float kProceduralWheelRadius = 0.31f;

constexpr char kSkateObjPath[] = "assets/models/skateboard_hq.obj";
constexpr char kSkateTexturePath[] = "assets/textures/skateboard_texture.ppm";
constexpr char kCharacterObjPath[] = "assets/models/character_hq.obj";
constexpr char kEnvironmentObjPath[] = "assets/models/skatepark_hq.obj";
constexpr char kSkyHdriTexturePath[] = "assets/textures/sky_hdri.ppm";

constexpr int SKATEBOARD_TEX_W = 256;
constexpr int SKATEBOARD_TEX_H = 256;

struct ObjFaceVertex
{
  int v = -1;
  int vt = -1;
  int vn = -1;
};

struct ObjTriangle
{
  ObjFaceVertex vertex[3];
  Vec3 fallbackNormal {0.0f, 1.0f, 0.0f};
  int materialIndex = -1;
};

struct ObjGroup
{
  std::string name;
  std::vector<ObjTriangle> triangles;
};

struct ObjMaterial
{
  std::string name;
  Vec3 diffuse {0.76f, 0.76f, 0.78f};
};

struct ObjModel
{
  std::vector<Vec3> positions;
  std::vector<std::array<float, 2>> texcoords;
  std::vector<Vec3> normals;
  std::vector<ObjGroup> groups;
  std::vector<ObjMaterial> materials;
  Vec3 minBounds {0.0f, 0.0f, 0.0f};
  Vec3 maxBounds {0.0f, 0.0f, 0.0f};
  bool hasBounds = false;
  GLuint textureId = 0;
  bool loaded = false;
};

enum class AgentRole
{
  VisualUnderstanding,
  Physics,
  Rigging,
  RenderExecution,
  BlenderBridge,
};

struct AgentNode
{
  AgentRole role = AgentRole::VisualUnderstanding;
  std::string capabilities;
  std::string lastAction;
  bool online = true;
  float latencyMs = 0.0f;
};

struct RenderJob
{
  std::string objPath;
  bool requiresPhysicsBake = true;
  bool requiresRigging = true;
  bool dispatchToRuntime = true;
};

struct AgentOrchestrator
{
  std::vector<AgentNode> agents;
  std::deque<std::string> eventLog;
  size_t maxLogEntries = 14;
  bool initialized = false;

  void logEvent(const std::string& event)
  {
    eventLog.push_back(event);
    if (eventLog.size() > maxLogEntries)
    {
      eventLog.pop_front();
    }
  }

  AgentNode* find(AgentRole role)
  {
    for (auto& agent : agents)
    {
      if (agent.role == role)
      {
        return &agent;
      }
    }
    return nullptr;
  }

  bool ensureOnline(AgentRole role, const std::string& missingMessage)
  {
    AgentNode* agent = find(role);
    if (!agent || !agent->online)
    {
      logEvent(missingMessage);
      return false;
    }
    return true;
  }

  void bootstrap()
  {
    if (initialized)
    {
      return;
    }

    agents = {
      {AgentRole::VisualUnderstanding, "Semantic mesh analysis and material intent extraction", "Idle", true, 6.0f},
      {AgentRole::Physics, "Collider synthesis and center-of-mass recommendations", "Idle", true, 4.0f},
      {AgentRole::Rigging, "Skeleton planning and control rig compatibility", "Idle", true, 5.0f},
      {AgentRole::RenderExecution, "Runtime LOD prep and GPU submission orchestration", "Idle", true, 3.0f},
      {AgentRole::BlenderBridge, "Blender scene sync and Python bridge messaging", "Idle", true, 8.0f},
    };
    initialized = true;
    logEvent("Agent mesh initialized with 5 specialists.");
  }

  bool submit(const RenderJob& job)
  {
    if (!initialized)
    {
      bootstrap();
    }

    if (!ensureOnline(AgentRole::VisualUnderstanding, "Visual Understanding agent unavailable."))
    {
      return false;
    }
    if (!ensureOnline(AgentRole::RenderExecution, "Render Execution agent unavailable."))
    {
      return false;
    }
    if (!ensureOnline(AgentRole::BlenderBridge, "Blender Bridge agent unavailable."))
    {
      return false;
    }
    if (job.requiresPhysicsBake && !ensureOnline(AgentRole::Physics, "Physics agent unavailable."))
    {
      return false;
    }
    if (job.requiresRigging && !ensureOnline(AgentRole::Rigging, "Rigging agent unavailable."))
    {
      return false;
    }

    // Cache agent pointers after availability has been ensured.
    auto* visualAgent = find(AgentRole::VisualUnderstanding);
    auto* renderExecAgent = job.dispatchToRuntime ? find(AgentRole::RenderExecution) : nullptr;
    auto* bridgeAgent = find(AgentRole::BlenderBridge);
    auto* physicsAgent = job.requiresPhysicsBake ? find(AgentRole::Physics) : nullptr;
    auto* riggingAgent = job.requiresRigging ? find(AgentRole::Rigging) : nullptr;

    if (visualAgent)
    {
      visualAgent->lastAction = "Parsed geometric semantics for " + job.objPath;
    }
    logEvent("Visual Understanding: inferred topology constraints for " + job.objPath);

    if (job.requiresPhysicsBake && physicsAgent)
    {
      physicsAgent->lastAction = "Generated collider envelopes and mass profile";
      logEvent("Physics: generated collider set + friction profile.");
    }

    if (job.requiresRigging && riggingAgent)
    {
      riggingAgent->lastAction = "Created rig contract and joint hierarchy proposal";
      logEvent("Rigging: authored animation-ready skeleton metadata.");
    }

    if (bridgeAgent)
    {
      bridgeAgent->lastAction = "Synced job payload to Blender automation bridge";
    }
    logEvent("Blender Bridge: dispatched sync command for editable scene graph.");

    if (job.dispatchToRuntime && renderExecAgent)
    {
      renderExecAgent->lastAction = "Prepared draw-ready runtime asset manifest";
      logEvent("Render Execution: queued runtime manifest for in-game renderer.");
    }

    return true;
  }
};

const char* agentRoleName(AgentRole role)
{
  switch (role)
  {
    case AgentRole::VisualUnderstanding:
      return "Visual Understanding";
    case AgentRole::Physics:
      return "Physics";
    case AgentRole::Rigging:
      return "Rigging";
    case AgentRole::RenderExecution:
      return "Render Execution";
    case AgentRole::BlenderBridge:
      return "Blender Bridge";
    default:
      return "Unknown";
  }
}

void drawDeck(float flex = 0.0f);

enum class GameState
{
  Menu,
  Play,
  Pause,
  Over,
  Win,
};

WorldState g_world;

GameState g_state = GameState::Menu;
ObjModel g_skateboardModel;
bool g_skateboardModelFallback = true;
ObjModel g_characterModel;
ObjModel g_environmentModel;
bool g_characterModelFallback = true;
bool g_environmentModelFallback = true;
GLuint g_skyHdriTextureId = 0;
bool g_skyHdriReady = false;

SDL_Window* g_window = nullptr;
SDL_GLContext g_glContext = nullptr;
bool g_running = true;
std::vector<bool> g_keys(SDL_NUM_SCANCODES, false);
bool g_jumpQueued = false;
bool g_paused = false;
float g_menuYaw = 0.0f;
float g_pausePulse = 0.0f;

float g_timeLeft = WORLD_TIME;
float g_messageTimer = 0.0f;
float g_flash = 0.0f;

float g_deltaBackup = 0.016f;
float g_lastFrame = 0.0f;
float g_fixedPhysicsAccumulator = 0.0f;
constexpr float FIXED_PHYSICS_STEP = 1.0f / 120.0f;
constexpr float MAX_PHYSICS_CATCHUP = 0.1f;

int g_windowW = 1360;
int g_windowH = 800;

std::string g_message = "Press Enter to Start";
std::string g_overlayTitle;
std::string g_overlayHint;
AgentOrchestrator g_agentOrchestrator;

TTF_Font* g_font = nullptr;

float olliePopEnvelope(float timer)
{
  if (timer <= 0.0f)
  {
    return 0.0f;
  }
  const float t = std::clamp(1.0f - (timer / OLLIE_POP_TIME), 0.0f, 1.0f);
  return std::sin(t * PI);
}

float nowSeconds()
{
  using clock_t = std::chrono::steady_clock;
  static const auto start = clock_t::now();
  return std::chrono::duration<float>(clock_t::now() - start).count();
}

std::string locateAssetPath(const char* relativePath)
{
  const std::filesystem::path rel = relativePath;

  std::vector<std::filesystem::path> candidates;
  candidates.push_back(rel);
  candidates.push_back(std::filesystem::path("..") / rel);
  candidates.push_back(std::filesystem::path("../..") / rel);

  try
  {
    const std::filesystem::path exe = std::filesystem::canonical("/proc/self/exe");
    const std::filesystem::path exeDir = exe.parent_path();
    candidates.push_back(exeDir / rel);
    candidates.push_back(exeDir / ".." / rel);
  }
  catch (...)
  {
    // Ignore path lookup failures and continue with remaining candidates.
  }

  for (const auto& c : candidates)
  {
    if (std::filesystem::exists(c))
    {
      return c.string();
    }
  }

  return relativePath;
}

bool readPPMToken(std::istream& in, std::string& out)
{
  while (in)
  {
    if (!(in >> std::ws))
    {
      return false;
    }

    const char c = static_cast<char>(in.peek());
    if (c == '#')
    {
      std::string discard;
      std::getline(in, discard);
      continue;
    }

    return static_cast<bool>(in >> out);
  }
  return false;
}

GLuint uploadRGBTexture(int width, int height, const unsigned char* pixels, GLint wrapT = GL_REPEAT)
{
  GLuint textureId = 0;
  glGenTextures(1, &textureId);
  glBindTexture(GL_TEXTURE_2D, textureId);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapT);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, pixels);
  return textureId;
}

bool loadPPMTexture(const std::string& path, GLuint& textureId)
{
  std::ifstream file(path, std::ios::binary);
  if (!file)
  {
    return false;
  }

  std::string magic;
  if (!readPPMToken(file, magic))
  {
    return false;
  }

  if (magic != "P3" && magic != "P6")
  {
    return false;
  }

  std::string token;
  if (!readPPMToken(file, token))
  {
    return false;
  }
  const int width = std::stoi(token);

  if (!readPPMToken(file, token))
  {
    return false;
  }
  const int height = std::stoi(token);

  if (!readPPMToken(file, token))
  {
    return false;
  }
  const int maxValue = std::stoi(token);
  if (width <= 0 || height <= 0 || maxValue <= 0 || maxValue > 65535)
  {
    return false;
  }

  std::vector<unsigned char> pixels(static_cast<size_t>(width * height * 3));
  if (magic == "P3")
  {
    const float scale = 255.0f / static_cast<float>(maxValue);
    for (size_t i = 0; i < pixels.size(); ++i)
    {
      if (!readPPMToken(file, token))
      {
        return false;
      }
      const int value = std::stoi(token);
      pixels[i] = static_cast<unsigned char>(std::clamp(static_cast<float>(value) * scale, 0.0f, 255.0f));
    }
  }
  else
  {
    char whitespace;
    file >> std::ws >> whitespace;
    file.unget();
    if (maxValue <= 255)
    {
      file.read(reinterpret_cast<char*>(pixels.data()), static_cast<std::streamsize>(pixels.size()));
      if (static_cast<size_t>(file.gcount()) != pixels.size())
      {
        return false;
      }
    }
    else
    {
      std::vector<unsigned short> raw16(width * height * 3);
      file.read(reinterpret_cast<char*>(raw16.data()), static_cast<std::streamsize>(raw16.size() * sizeof(unsigned short)));
      if (static_cast<size_t>(file.gcount()) != raw16.size() * sizeof(unsigned short))
      {
        return false;
      }

      const float scale = 255.0f / static_cast<float>(maxValue);
      for (size_t i = 0; i < raw16.size(); ++i)
      {
        pixels[i] = static_cast<unsigned char>(std::clamp(static_cast<float>(raw16[i]) * scale, 0.0f, 255.0f));
      }
    }
  }

  textureId = uploadRGBTexture(width, height, pixels.data());
  return true;
}

GLuint createFallbackSkateTexture()
{
  std::vector<unsigned char> pixels(static_cast<size_t>(SKATEBOARD_TEX_W * SKATEBOARD_TEX_H * 3));
  for (int y = 0; y < SKATEBOARD_TEX_H; ++y)
  {
    for (int x = 0; x < SKATEBOARD_TEX_W; ++x)
    {
      const bool grid = ((x / 16 + y / 16) & 1) == 0;
      const bool edge = x < 8 || y < 8 || x > SKATEBOARD_TEX_W - 9 || y > SKATEBOARD_TEX_H - 9;

      const float stripe = static_cast<float>((y + (x / 6)) % 18 < 3);
      const int idx = (y * SKATEBOARD_TEX_W + x) * 3;
      unsigned char r = static_cast<unsigned char>(35 + static_cast<int>(grid) * 18);
      unsigned char g = static_cast<unsigned char>(25 + static_cast<int>(grid) * 10);
      unsigned char b = static_cast<unsigned char>(18 + static_cast<int>(stripe) * 30);

      if (edge)
      {
        r = static_cast<unsigned char>(std::min(255, r + 18));
        g = static_cast<unsigned char>(std::min(255, g + 14));
        b = static_cast<unsigned char>(std::min(255, b + 8));
      }

      pixels[idx] = r;
      pixels[idx + 1] = g;
      pixels[idx + 2] = b;
    }
  }

  return uploadRGBTexture(SKATEBOARD_TEX_W, SKATEBOARD_TEX_H, pixels.data());
}

GLuint createFallbackSkyTexture()
{
  constexpr int kSkyW = 1024;
  constexpr int kSkyH = 512;
  std::vector<unsigned char> pixels(static_cast<size_t>(kSkyW * kSkyH * 3));
  for (int y = 0; y < kSkyH; ++y)
  {
    const float v = static_cast<float>(y) / static_cast<float>(kSkyH - 1);
    for (int x = 0; x < kSkyW; ++x)
    {
      const float u = static_cast<float>(x) / static_cast<float>(kSkyW - 1);
      const float sunDx = u - 0.73f;
      const float sunDy = v - 0.28f;
      const float sun = std::exp(-(sunDx * sunDx + sunDy * sunDy) * 820.0f);
      const float haze = std::exp(-(sunDx * sunDx + sunDy * sunDy) * 180.0f);

      const float skyR = 0.08f + (1.0f - v) * 0.38f + sun * 1.8f + haze * 0.45f;
      const float skyG = 0.16f + (1.0f - v) * 0.36f + sun * 1.35f + haze * 0.26f;
      const float skyB = 0.35f + (1.0f - v) * 0.36f + sun * 0.9f;

      const int idx = (y * kSkyW + x) * 3;
      pixels[idx + 0] = static_cast<unsigned char>(std::clamp(skyR, 0.0f, 1.0f) * 255.0f);
      pixels[idx + 1] = static_cast<unsigned char>(std::clamp(skyG, 0.0f, 1.0f) * 255.0f);
      pixels[idx + 2] = static_cast<unsigned char>(std::clamp(skyB, 0.0f, 1.0f) * 255.0f);
    }
  }

  return uploadRGBTexture(kSkyW, kSkyH, pixels.data(), GL_CLAMP_TO_EDGE);
}

bool parseObjIndex(const std::string& token, int count, int& out)
{
  try
  {
    const int parsed = std::stoi(token);
    if (parsed == 0)
    {
      return false;
    }

    if (parsed > 0)
    {
      if (parsed > count)
      {
        return false;
      }
      out = parsed - 1;
    }
    else
    {
      const int absIndex = count + parsed;
      if (absIndex < 0)
      {
        return false;
      }
      out = absIndex;
    }
    return out >= 0;
  }
  catch (...)
  {
    return false;
  }
}

bool parseFaceToken(
  const std::string& token,
  const ObjModel& model,
  ObjFaceVertex& outVertex)
{
  std::string a, b, c;
  const size_t first = token.find('/');
  const size_t last = token.rfind('/');

  if (first == std::string::npos)
  {
    if (!parseObjIndex(token, static_cast<int>(model.positions.size()), outVertex.v))
    {
      return false;
    }
    return true;
  }

  a = token.substr(0, first);
  if (last == first)
  {
    b = token.substr(first + 1);
  }
  else
  {
    b = token.substr(first + 1, last - first - 1);
    c = token.substr(last + 1);
  }

  if (!parseObjIndex(a, static_cast<int>(model.positions.size()), outVertex.v))
  {
    return false;
  }

  if (!b.empty())
  {
    parseObjIndex(b, static_cast<int>(model.texcoords.size()), outVertex.vt);
  }
  if (!c.empty())
  {
    parseObjIndex(c, static_cast<int>(model.normals.size()), outVertex.vn);
  }

  return true;
}

ObjGroup& getOrCreateObjGroup(ObjModel& model, const std::string& name)
{
  if (model.groups.empty() || model.groups.back().name != name)
  {
    model.groups.push_back({name, {}});
  }
  return model.groups.back();
}

std::string trimAscii(std::string value)
{
  const auto isSpace = [](unsigned char c)
  {
    return std::isspace(c) != 0;
  };

  value.erase(value.begin(), std::find_if(value.begin(), value.end(), [&](unsigned char c) { return !isSpace(c); }));
  value.erase(std::find_if(value.rbegin(), value.rend(), [&](unsigned char c) { return !isSpace(c); }).base(), value.end());
  return value;
}

void updateObjBounds(ObjModel& model, const Vec3& point)
{
  if (!model.hasBounds)
  {
    model.minBounds = point;
    model.maxBounds = point;
    model.hasBounds = true;
    return;
  }

  model.minBounds.x = std::min(model.minBounds.x, point.x);
  model.minBounds.y = std::min(model.minBounds.y, point.y);
  model.minBounds.z = std::min(model.minBounds.z, point.z);
  model.maxBounds.x = std::max(model.maxBounds.x, point.x);
  model.maxBounds.y = std::max(model.maxBounds.y, point.y);
  model.maxBounds.z = std::max(model.maxBounds.z, point.z);
}

void recomputeObjBounds(ObjModel& model)
{
  model.hasBounds = false;
  for (const Vec3& v : model.positions)
  {
    updateObjBounds(model, v);
  }
}

int findObjMaterialIndex(const ObjModel& model, const std::string& name)
{
  for (size_t i = 0; i < model.materials.size(); ++i)
  {
    if (model.materials[i].name == name)
    {
      return static_cast<int>(i);
    }
  }
  return -1;
}

int getOrCreateObjMaterial(ObjModel& model, const std::string& name)
{
  const int existing = findObjMaterialIndex(model, name);
  if (existing >= 0)
  {
    return existing;
  }

  model.materials.push_back({name, {0.76f, 0.76f, 0.78f}});
  return static_cast<int>(model.materials.size() - 1);
}

bool loadObjMaterialLibrary(const std::filesystem::path& objPath, const std::string& rawLibPath, ObjModel& model)
{
  const std::string libPathTrimmed = trimAscii(rawLibPath);
  if (libPathTrimmed.empty())
  {
    return false;
  }

  const std::filesystem::path mtlPath = objPath.parent_path() / libPathTrimmed;
  std::ifstream mtlFile(mtlPath);
  if (!mtlFile)
  {
    return false;
  }

  ObjMaterial* currentMaterial = nullptr;
  std::string line;
  while (std::getline(mtlFile, line))
  {
    if (!line.empty() && line.back() == '\r')
    {
      line.pop_back();
    }
    if (line.empty() || line[0] == '#')
    {
      continue;
    }

    std::stringstream ss(line);
    std::string type;
    ss >> type;

    if (type == "newmtl")
    {
      std::string materialName;
      std::getline(ss, materialName);
      materialName = trimAscii(materialName);
      if (materialName.empty())
      {
        currentMaterial = nullptr;
        continue;
      }
      const int index = getOrCreateObjMaterial(model, materialName);
      currentMaterial = &model.materials[index];
    }
    else if (type == "Kd" && currentMaterial)
    {
      ss >> currentMaterial->diffuse.x >> currentMaterial->diffuse.y >> currentMaterial->diffuse.z;
      currentMaterial->diffuse.x = std::clamp(currentMaterial->diffuse.x, 0.0f, 1.0f);
      currentMaterial->diffuse.y = std::clamp(currentMaterial->diffuse.y, 0.0f, 1.0f);
      currentMaterial->diffuse.z = std::clamp(currentMaterial->diffuse.z, 0.0f, 1.0f);
    }
    else if (type == "map_Kd" && model.textureId == 0)
    {
      std::string textureRelPath;
      std::getline(ss, textureRelPath);
      textureRelPath = trimAscii(textureRelPath);
      if (textureRelPath.empty())
      {
        continue;
      }

      const std::filesystem::path texturePath = mtlPath.parent_path() / textureRelPath;
      GLuint importedTexture = 0;
      if (loadPPMTexture(texturePath.string(), importedTexture))
      {
        model.textureId = importedTexture;
      }
    }
  }

  return true;
}

bool loadObjModel(const std::string& path, ObjModel& model)
{
  std::ifstream file(path);
  if (!file)
  {
    std::fprintf(stderr, "Could not open skateboard model: %s\n", path.c_str());
    return false;
  }

  model = {};
  const std::filesystem::path objPath(path);
  std::string currentGroupName = "board";
  ObjGroup* currentGroup = &getOrCreateObjGroup(model, currentGroupName);
  int currentMaterialIndex = -1;

  std::string line;
  while (std::getline(file, line))
  {
    if (!line.empty() && line.back() == '\r')
    {
      line.pop_back();
    }
    if (line.empty() || line[0] == '#')
    {
      continue;
    }

    std::stringstream ss(line);
    std::string type;
    ss >> type;

    if (type == "v")
    {
      Vec3 v;
      ss >> v.x >> v.y >> v.z;
      model.positions.push_back(v);
      updateObjBounds(model, v);
    }
    else if (type == "vt")
    {
      std::array<float, 2> tc{};
      ss >> tc[0] >> tc[1];
      model.texcoords.push_back(tc);
    }
    else if (type == "vn")
    {
      Vec3 n;
      ss >> n.x >> n.y >> n.z;
      model.normals.push_back(n);
    }
    else if (type == "g" || type == "o")
    {
      ss >> currentGroupName;
      currentGroupName = trimAscii(currentGroupName);
      if (currentGroupName.empty())
      {
        currentGroupName = "board";
      }
      currentGroup = &getOrCreateObjGroup(model, currentGroupName);
    }
    else if (type == "mtllib")
    {
      std::string libraryPath;
      std::getline(ss, libraryPath);
      loadObjMaterialLibrary(objPath, libraryPath, model);
    }
    else if (type == "usemtl")
    {
      std::string materialName;
      std::getline(ss, materialName);
      materialName = trimAscii(materialName);
      currentMaterialIndex = materialName.empty() ? -1 : getOrCreateObjMaterial(model, materialName);
    }
    else if (type == "f")
    {
      std::vector<ObjFaceVertex> faceVertices;
      std::string token;
      while (ss >> token)
      {
        ObjFaceVertex vtx;
        if (parseFaceToken(token, model, vtx))
        {
          faceVertices.push_back(vtx);
        }
      }

      if (faceVertices.size() < 3)
      {
        continue;
      }

      for (size_t i = 1; i + 1 < faceVertices.size(); ++i)
      {
        ObjTriangle tri;
        tri.vertex[0] = faceVertices[0];
        tri.vertex[1] = faceVertices[i];
        tri.vertex[2] = faceVertices[i + 1];
        tri.materialIndex = currentMaterialIndex;

        const auto& a = tri.vertex[0];
        const auto& b = tri.vertex[1];
        const auto& c = tri.vertex[2];

        if (a.v >= 0 && b.v >= 0 && c.v >= 0)
        {
          const Vec3 e1 = model.positions[b.v] - model.positions[a.v];
          const Vec3 e2 = model.positions[c.v] - model.positions[a.v];
          tri.fallbackNormal = safeNormalize(cross(e1, e2));
        }

        currentGroup->triangles.push_back(tri);
      }
    }
  }

  if (model.positions.empty() || model.groups.empty())
  {
    std::fprintf(stderr, "Loaded skateboard model was empty: %s\n", path.c_str());
    return false;
  }
  if (!model.hasBounds)
  {
    recomputeObjBounds(model);
  }

  for (auto& group : model.groups)
  {
    if (!group.triangles.empty())
    {
      model.loaded = true;
      break;
    }
  }

  return model.loaded;
}

bool hasObjGroup(const ObjModel& model, const char* name)
{
  if (!name)
  {
    return false;
  }

  for (const auto& group : model.groups)
  {
    if (group.name == name)
    {
      return true;
    }
  }
  return false;
}

bool hasSkateboardPartGroups(const ObjModel& model)
{
  return hasObjGroup(model, "Deck") ||
    hasObjGroup(model, "deck") ||
    hasObjGroup(model, "TruckFront") ||
    hasObjGroup(model, "TruckRear") ||
    hasObjGroup(model, "Truck") ||
    hasObjGroup(model, "Wheel") ||
    hasObjGroup(model, "WheelFL") ||
    hasObjGroup(model, "WheelFR") ||
    hasObjGroup(model, "WheelRL") ||
    hasObjGroup(model, "WheelRR");
}

void normalizeFullSkateboardMesh(ObjModel& model)
{
  if (!model.hasBounds)
  {
    recomputeObjBounds(model);
  }
  if (!model.hasBounds)
  {
    return;
  }

  float minAxis[3] = {model.minBounds.x, model.minBounds.y, model.minBounds.z};
  float maxAxis[3] = {model.maxBounds.x, model.maxBounds.y, model.maxBounds.z};
  float spanAxis[3] = {
    std::max(0.0001f, maxAxis[0] - minAxis[0]),
    std::max(0.0001f, maxAxis[1] - minAxis[1]),
    std::max(0.0001f, maxAxis[2] - minAxis[2]),
  };
  float centerAxis[3] = {
    (minAxis[0] + maxAxis[0]) * 0.5f,
    (minAxis[1] + maxAxis[1]) * 0.5f,
    (minAxis[2] + maxAxis[2]) * 0.5f,
  };

  int lengthAxis = 0;
  int heightAxis = 0;
  for (int i = 1; i < 3; ++i)
  {
    if (spanAxis[i] > spanAxis[lengthAxis])
    {
      lengthAxis = i;
    }
    if (spanAxis[i] < spanAxis[heightAxis])
    {
      heightAxis = i;
    }
  }
  int widthAxis = 3 - lengthAxis - heightAxis;
  if (widthAxis < 0 || widthAxis > 2)
  {
    widthAxis = 1;
  }

  for (Vec3& p : model.positions)
  {
    const float src[3] = {p.x, p.y, p.z};
    p.x = src[widthAxis] - centerAxis[widthAxis];
    p.y = src[heightAxis] - centerAxis[heightAxis];
    p.z = src[lengthAxis] - centerAxis[lengthAxis];
  }
  recomputeObjBounds(model);

  const float rangeX = std::max(0.0001f, model.maxBounds.x - model.minBounds.x);
  const float rangeY = std::max(0.0001f, model.maxBounds.y - model.minBounds.y);
  const float rangeZ = std::max(0.0001f, model.maxBounds.z - model.minBounds.z);

  const float targetWidth = kProceduralDeckWidth * 0.95f;
  const float targetHeight = 0.66f;
  const float targetLength = kProceduralDeckLength;
  const float sx = targetWidth / rangeX;
  const float sy = targetHeight / rangeY;
  const float sz = targetLength / rangeZ;

  for (Vec3& p : model.positions)
  {
    p.x *= sx;
    p.y *= sy;
    p.z *= sz;
  }
  recomputeObjBounds(model);

  const float desiredBottom = -0.55f;
  const float yShift = desiredBottom - model.minBounds.y;
  for (Vec3& p : model.positions)
  {
    p.y += yShift;
  }
  recomputeObjBounds(model);
}

bool ensureSkateboardModelAssets()
{
  const std::string modelPath = locateAssetPath(kSkateObjPath);
  if (!loadObjModel(modelPath, g_skateboardModel))
  {
    g_skateboardModelFallback = true;
    g_skateboardModel.loaded = false;
    g_skateboardModel.textureId = createFallbackSkateTexture();
    return false;
  }

  g_skateboardModelFallback = false;
  const bool hasPartGroups = hasSkateboardPartGroups(g_skateboardModel);
  if (!hasPartGroups)
  {
    normalizeFullSkateboardMesh(g_skateboardModel);
  }

  if (hasPartGroups && g_skateboardModel.textureId == 0)
  {
    const std::string texturePath = locateAssetPath(kSkateTexturePath);
    if (!loadPPMTexture(texturePath, g_skateboardModel.textureId))
    {
      g_skateboardModel.textureId = createFallbackSkateTexture();
    }
  }
  return true;
}

bool ensureObjAsset(const char* relativePath, ObjModel& model)
{
  const std::string modelPath = locateAssetPath(relativePath);
  model = {};
  if (!loadObjModel(modelPath, model))
  {
    model.loaded = false;
    model.textureId = 0;
    return false;
  }
  return true;
}

bool ensureHdriSkyTexture()
{
  const std::string texturePath = locateAssetPath(kSkyHdriTexturePath);
  if (!loadPPMTexture(texturePath, g_skyHdriTextureId))
  {
    g_skyHdriTextureId = createFallbackSkyTexture();
    g_skyHdriReady = false;
    return false;
  }

  glBindTexture(GL_TEXTURE_2D, g_skyHdriTextureId);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  g_skyHdriReady = true;
  return true;
}

void drawObjModel(const ObjModel& model, const char* targetGroup = nullptr, bool useTexture = true)
{
  if (!model.loaded)
  {
    return;
  }

  const GLboolean cullWasEnabled = glIsEnabled(GL_CULL_FACE);
  if (cullWasEnabled)
  {
    glDisable(GL_CULL_FACE);
  }

  const GLboolean textureWasEnabled = glIsEnabled(GL_TEXTURE_2D);
  const bool hasTexture = (model.textureId != 0) && useTexture;
  const bool applyMaterialColors = !hasTexture && !model.materials.empty();
  if (hasTexture)
  {
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, model.textureId);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
  }
  else if (textureWasEnabled)
  {
    glDisable(GL_TEXTURE_2D);
  }

  const std::string selected = targetGroup ? targetGroup : "";
  for (const auto& group : model.groups)
  {
    if (targetGroup && group.name != selected)
    {
      continue;
    }

    glBegin(GL_TRIANGLES);
    for (const auto& tri : group.triangles)
    {
      if (applyMaterialColors)
      {
        Vec3 triColor{0.76f, 0.76f, 0.78f};
        if (tri.materialIndex >= 0 && tri.materialIndex < static_cast<int>(model.materials.size()))
        {
          triColor = model.materials[tri.materialIndex].diffuse;
        }
        glColor3f(triColor.x, triColor.y, triColor.z);
      }

      for (int i = 0; i < 3; ++i)
      {
        const auto& fv = tri.vertex[i];
        if (fv.vn >= 0 && fv.vn < static_cast<int>(model.normals.size()))
        {
          const auto& n = model.normals[fv.vn];
          glNormal3f(n.x, n.y, n.z);
        }
        else
        {
          glNormal3f(tri.fallbackNormal.x, tri.fallbackNormal.y, tri.fallbackNormal.z);
        }

        if (hasTexture && fv.vt >= 0 && fv.vt < static_cast<int>(model.texcoords.size()))
        {
          const auto& uv = model.texcoords[fv.vt];
          glTexCoord2f(uv[0], 1.0f - uv[1]);
        }
        else
        {
          glTexCoord2f(0.0f, 0.0f);
        }

        if (fv.v >= 0 && fv.v < static_cast<int>(model.positions.size()))
        {
          const Vec3& p = model.positions[fv.v];
          glVertex3f(p.x, p.y, p.z);
        }
      }
    }
    glEnd();
  }

  if (hasTexture)
  {
    glDisable(GL_TEXTURE_2D);
  }
  else if (textureWasEnabled)
  {
    glEnable(GL_TEXTURE_2D);
  }
  if (cullWasEnabled)
  {
    glEnable(GL_CULL_FACE);
  }
}

void drawSkateboardModelFallback()
{
  if (g_skateboardModelFallback || !g_skateboardModel.loaded || !hasObjGroup(g_skateboardModel, "Deck"))
  {
    drawDeck();
    return;
  }

  drawObjModel(g_skateboardModel, "Deck", true);
}

bool loadDefaultFont(int px)
{
  if (g_font)
  {
    return true;
  }

  static const char* extraCandidates[] = {
    "/usr/share/fonts/google-noto-vf/NotoSans[wght].ttf",
    "/usr/share/fonts/google-noto-vf/NotoSans-Regular.ttf",
    "/usr/share/fonts/google-noto-vf/NotoSans-Regular.otf",
    "/usr/share/fonts/truetype/google-fonts/NotoSans-Regular.ttf",
    "/usr/share/fonts/google-noto/NotoSans-Regular.ttf",
    "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
    "/usr/share/fonts/gnu-free/FreeSans.ttf",
    "/usr/share/fonts/croscore/arial.ttf",
  };

  static const char* candidates[] = {
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    "/usr/share/fonts/TTF/DejaVuSans.ttf",
    "/usr/share/fonts/dejavu/DejaVuSans.ttf",
    "/usr/share/fonts/truetype/freefont/FreeSans.ttf",
    "/usr/share/fonts/freefont/FreeSans.ttf",
  };

  for (const char* path : candidates)
  {
    if (!std::filesystem::exists(path))
    {
      continue;
    }

    g_font = TTF_OpenFont(path, px);
    if (g_font)
    {
      return true;
    }
  }

  for (const char* path : extraCandidates)
  {
    if (!std::filesystem::exists(path))
    {
      continue;
    }

    g_font = TTF_OpenFont(path, px);
    if (g_font)
    {
      return true;
    }
  }

  return false;
}

void drawText(int x, int y, const std::string& s)
{
  if (s.empty() || !g_font)
  {
    return;
  }

  SDL_Color color{240, 250, 255, 255};
  SDL_Surface* textSurface = TTF_RenderUTF8_Blended(g_font, s.c_str(), color);
  if (!textSurface)
  {
    return;
  }

  SDL_Surface* converted = SDL_ConvertSurfaceFormat(textSurface, SDL_PIXELFORMAT_RGBA32, 0);
  SDL_FreeSurface(textSurface);
  if (!converted)
  {
    return;
  }

  GLuint texture = 0;
  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D, texture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, converted->w, converted->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, converted->pixels);

  const float w = static_cast<float>(converted->w);
  const float h = static_cast<float>(converted->h);
  const float x0 = static_cast<float>(x);
  const float y0 = static_cast<float>(y) - h;

  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, texture);
  glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
  glBegin(GL_QUADS);
  glTexCoord2f(0.0f, 1.0f);
  glVertex2f(x0, y0);
  glTexCoord2f(1.0f, 1.0f);
  glVertex2f(x0 + w, y0);
  glTexCoord2f(1.0f, 0.0f);
  glVertex2f(x0 + w, y0 + h);
  glTexCoord2f(0.0f, 0.0f);
  glVertex2f(x0, y0 + h);
  glEnd();
  glDisable(GL_TEXTURE_2D);

  SDL_FreeSurface(converted);
  glDeleteTextures(1, &texture);
}

void drawTextWithShadow(int x, int y, const std::string& s, float alpha = 1.0f)
{
  if (s.empty())
  {
    return;
  }

  glColor4f(0.02f, 0.04f, 0.09f, 0.85f * alpha);
  drawText(x + 1, y - 1, s);
  glColor4f(0.97f, 0.99f, 1.0f, alpha);
  drawText(x, y, s);
}

void setMessage(const std::string& text, float seconds = 1.5f)
{
  g_message = text;
  g_messageTimer = std::max(g_messageTimer, seconds);
}

void showOverlay(const std::string& title, const std::string& hint)
{
  g_overlayTitle = title;
  g_overlayHint = hint;
}

void makeWorld()
{
  initializeWorld(g_world, g_player.position);
}

void resetRun()
{
  g_player.position = {0.0f, 4.0f, 0.0f};
  g_player.velocity = {0.0f, 0.0f, 6.0f};
  g_player.yaw = 0.0f;
  g_player.grounded = false;
  g_player.grinding = false;
  g_player.grindCombo = 0;
  g_player.grindTime = 0.0f;
  g_player.grindHintTimer = 0.0f;
  g_player.currentGrindIndex = -1;
  g_player.deckFlex = 0.0f;
  g_player.airTime = 0.0f;
  g_player.ollieAnim = 0.0f;
  g_player.manualBalance = 0.0f;
  g_player.balanceVelocity = 0.0f;
  g_player.comboEngine.reset();
  g_player.trickFsm.reset(true);
  g_player.trickState = TrickState::Grounded;
  g_player.bailedThisFrame = false;
  g_player.invuln = 0.0f;
  g_player.lives = 3;
  g_player.score = 0;
  g_player.distance = 0.0f;
  g_timeLeft = WORLD_TIME;
  g_flash = 0.0f;
  g_state = GameState::Play;
  g_paused = false;
  showOverlay("", "");
  g_menuYaw = 0.0f;

  resetWorld(g_world, g_player.position);

  g_message = "Ride the line";
  g_messageTimer = 0.75f;
}

void loseLife()
{
  g_player.bailedThisFrame = true;
  g_player.comboEngine.onBail();
  g_player.grinding = false;
  g_player.grindCombo = 0;
  g_player.grindTime = 0.0f;
  g_player.currentGrindIndex = -1;
  g_player.deckFlex = 0.0f;
  g_player.ollieAnim = 0.0f;
  g_player.manualBalance = 0.0f;
  g_player.balanceVelocity = 0.0f;
  --g_player.lives;
  g_player.invuln = 1.2f;
  g_flash = 0.18f;
  g_player.velocity.x *= 0.33f;
  g_player.velocity.z *= 0.33f;
  g_player.velocity.y *= 0.25f;
  setMessage("Clean Hit! -1 Life", 1.1f);

  if (g_player.lives <= 0)
  {
    g_state = GameState::Over;
    showOverlay("GAME OVER", "Score " + std::to_string(g_player.score) + " - Press Enter or R to Replay");
  }
}

void winRun()
{
  g_state = GameState::Win;
  showOverlay("TIME FINISHED - GREAT RIDE!", "Score " + std::to_string(g_player.score) + " - Press Enter or R to Replay");
}

bool forwardPressed()
{
  return g_keys[SDL_SCANCODE_W] || g_keys[SDL_SCANCODE_UP];
}

bool backwardPressed()
{
  return g_keys[SDL_SCANCODE_S] || g_keys[SDL_SCANCODE_DOWN];
}

bool leftPressed()
{
  return g_keys[SDL_SCANCODE_A] || g_keys[SDL_SCANCODE_LEFT];
}

bool rightPressed()
{
  return g_keys[SDL_SCANCODE_D] || g_keys[SDL_SCANCODE_RIGHT];
}

const char* trickStateLabel(TrickState state)
{
  switch (state)
  {
    case TrickState::Grounded:
      return "GROUNDED";
    case TrickState::Ollie:
      return "OLLIE";
    case TrickState::FlipTrick:
      return "FLIP";
    case TrickState::Grinding:
      return "GRIND";
    case TrickState::Bailing:
      return "BAIL";
    default:
      return "UNKNOWN";
  }
}

std::string activeTrickLabel()
{
  if (g_player.trickState == TrickState::FlipTrick)
  {
    switch (g_player.trickFsm.activeFlip())
    {
      case FlipTrickType::Kickflip:
        return "KICKFLIP";
      case FlipTrickType::Heelflip:
        return "HEELFLIP";
      case FlipTrickType::None:
      default:
        return "FLIP";
    }
  }

  return trickStateLabel(g_player.trickState);
}

InputState buildInputState()
{
  InputState input;
  input.forwardPressed = forwardPressed();
  input.backwardPressed = backwardPressed();
  input.turnAxis = (rightPressed() ? 1.0f : 0.0f) - (leftPressed() ? 1.0f : 0.0f);
  input.jumpPressed = g_jumpQueued;
  input.flipPressed = g_keys[SDL_SCANCODE_LSHIFT] || g_keys[SDL_SCANCODE_RSHIFT];
  return input;
}

void recycleWorld(float dt)
{
  recycleWorld(g_world, g_player.position, dt);
}

bool intersectsAabb(const Vec3& centerA, const Vec3& halfExtentsA, const Vec3& centerB, const Vec3& halfExtentsB)
{
  return std::fabs(centerA.x - centerB.x) <= halfExtentsA.x + halfExtentsB.x &&
    std::fabs(centerA.y - centerB.y) <= halfExtentsA.y + halfExtentsB.y &&
    std::fabs(centerA.z - centerB.z) <= halfExtentsA.z + halfExtentsB.z;
}

bool sweptAabbHit(
  const Vec3& start,
  const Vec3& end,
  const Vec3& playerHalfExtents,
  const Vec3& obstacleCenter,
  const Vec3& obstacleHalfExtents)
{
  const Vec3 expandedHalf {
    obstacleHalfExtents.x + playerHalfExtents.x,
    obstacleHalfExtents.y + playerHalfExtents.y,
    obstacleHalfExtents.z + playerHalfExtents.z,
  };

  const Vec3 delta {end.x - start.x, end.y - start.y, end.z - start.z};

  float entry = 0.0f;
  float exit = 1.0f;

  auto sweepAxis = [&](float startAxis, float deltaAxis, float minAxis, float maxAxis)
  {
    if (std::fabs(deltaAxis) < 0.00001f)
    {
      return startAxis >= minAxis && startAxis <= maxAxis;
    }

    const float inv = 1.0f / deltaAxis;
    float t0 = (minAxis - startAxis) * inv;
    float t1 = (maxAxis - startAxis) * inv;

    if (t0 > t1)
    {
      std::swap(t0, t1);
    }

    entry = std::max(entry, t0);
    exit = std::min(exit, t1);
    return entry <= exit;
  };

  const float minX = obstacleCenter.x - expandedHalf.x;
  const float maxX = obstacleCenter.x + expandedHalf.x;
  const float minY = obstacleCenter.y - expandedHalf.y;
  const float maxY = obstacleCenter.y + expandedHalf.y;
  const float minZ = obstacleCenter.z - expandedHalf.z;
  const float maxZ = obstacleCenter.z + expandedHalf.z;

  if (!sweepAxis(start.x, delta.x, minX, maxX))
  {
    return false;
  }
  if (!sweepAxis(start.y, delta.y, minY, maxY))
  {
    return false;
  }
  if (!sweepAxis(start.z, delta.z, minZ, maxZ))
  {
    return false;
  }

  return exit >= 0.0f && entry <= 1.0f;
}

void updateObstaclesAndCoins(float dt)
{
  // Track the player's position from the previous frame to get an accurate swept segment.
  static Vec3 s_previousPlayerPos = g_player.position;

  const Vec3 previousPlayerPos = s_previousPlayerPos;
  const Vec3 playerPos = g_player.position;
  s_previousPlayerPos = playerPos;
  const Vec3 playerHalfExtents {BOARD_RADIUS, BOARD_RADIUS, BOARD_RADIUS};
  const float playerSpeed = length2D(g_player.velocity);
  int bestRailIndex = -1;
  float bestRailScore = 1.0e9f;

  for (size_t i = 0; i < g_world.obstacles.size(); ++i)
  {
    auto& obstacle = g_world.obstacles[i];
    if (!obstacle.active)
    {
      continue;
    }

    const float testY = terrainHeight(obstacle.position.x, obstacle.position.z) + obstacle.radius;
    obstacle.position.y = std::fma(0.2f, (testY - obstacle.position.y), obstacle.position.y);

    const Vec3 d{playerPos.x - obstacle.position.x, playerPos.y - obstacle.position.y, playerPos.z - obstacle.position.z};
    const bool collidesNow = intersectsAabb(playerPos, playerHalfExtents, obstacle.position, obstacle.collisionHalfExtents);
    const bool sweptCollision = sweptAabbHit(
      previousPlayerPos,
      playerPos,
      playerHalfExtents,
      obstacle.position,
      obstacle.collisionHalfExtents);

    if (!obstacle.rail && (collidesNow || sweptCollision) && g_player.invuln <= 0.0f && obstacle.hitCooldown <= 0.0f)
    {
      obstacle.hitCooldown = 1.0f;
      loseLife();
      break;
    }

    if (obstacle.rail && (collidesNow || sweptCollision) && g_player.invuln <= 0.0f && obstacle.hitCooldown <= 0.0f &&
        !canGrindOnRail(obstacle, playerPos, playerSpeed))
    {
      obstacle.hitCooldown = 1.0f;
      loseLife();
      break;
    }

    if (obstacle.rail && canGrindOnRail(obstacle, playerPos, playerSpeed))
    {
      const float score = d.x * d.x + d.z * d.z;
      if (score < bestRailScore)
      {
        bestRailScore = score;
        bestRailIndex = static_cast<int>(i);
      }
    }
  }

  const bool canGrind = g_player.invuln <= 0.0f && bestRailIndex >= 0 && g_player.grounded;
  if (canGrind)
  {
    const bool continuingRail = g_player.grinding && g_player.currentGrindIndex == bestRailIndex;

    if (!continuingRail)
    {
      const int combo = std::max(1, g_player.grindCombo + 1);
      g_player.grindCombo = combo;
      const int entryScore = GRIND_ENTRY_BONUS + combo * 20;
      g_player.comboEngine.bumpMultiplier(0.22f);
      const int entryAward = g_player.comboEngine.addPoints(entryScore);
      g_player.grindHintTimer = 0.35f;
      setMessage("GRIND +" + std::to_string(entryAward) + "x" + std::to_string(combo), 0.9f);
      g_player.grindTime = 0.0f;
      g_player.grindScoreCarry = 0.0f;
    }

    g_player.grinding = true;
    g_player.currentGrindIndex = bestRailIndex;
    g_player.grindTime += dt;
    g_player.grindHintTimer = std::max(0.0f, g_player.grindHintTimer - dt);
    g_player.grindScoreCarry += GRIND_SCORE_RATE * dt;

    const int grindScore = static_cast<int>(std::floor(g_player.grindScoreCarry));
    if (grindScore > 0)
    {
      g_player.comboEngine.addPoints(grindScore);
      g_player.grindScoreCarry -= static_cast<float>(grindScore);
    }

    g_player.velocity.z *= GRIND_SPEED_MULT;
    g_player.velocity.x *= 0.88f;
    if (g_player.velocity.y > 0.0f)
    {
      g_player.velocity.y = 0.0f;
    }
    g_player.airTime = 0.0f;
    g_player.grounded = true;
    if (g_player.grindHintTimer <= 0.0f)
    {
      setMessage("GRINDING", 0.35f);
      g_player.grindHintTimer = 0.42f;
    }
  }
  else
  {
    if (g_player.grinding)
    {
      const int exitScore = GRIND_EXIT_BONUS + g_player.grindCombo * 30;
      g_player.comboEngine.bumpMultiplier(0.12f);
      const int exitAward = g_player.comboEngine.addPoints(exitScore);
      setMessage("GRIND END +" + std::to_string(exitAward), 0.55f);
    }

    g_player.grinding = false;
    g_player.currentGrindIndex = -1;
    g_player.grindTime = 0.0f;
    g_player.grindHintTimer = 0.0f;
    g_player.grindScoreCarry = 0.0f;
    g_player.grindCombo = 0;

    if (g_player.invuln <= 0.0f && g_player.velocity.y <= 0.0f)
    {
      for (const auto& obstacle : g_world.obstacles)
      {
        if (!obstacle.active || !obstacle.rail)
        {
          continue;
        }

        const bool collidesNow = intersectsAabb(playerPos, playerHalfExtents, obstacle.position, obstacle.collisionHalfExtents);
        const bool sweptCollision = sweptAabbHit(
          previousPlayerPos,
          playerPos,
          playerHalfExtents,
          obstacle.position,
          obstacle.collisionHalfExtents);
        if ((collidesNow || sweptCollision) && obstacle.hitCooldown <= 0.0f)
        {
          loseLife();
          break;
        }
      }
    }
  }

  for (auto& coin : g_world.coins)
  {
    if (!coin.active)
    {
      continue;
    }

    coin.spin += dt * 1.1f;
    coin.position.y = terrainHeight(coin.position.x, coin.position.z) + 1.15f + std::sin(nowSeconds() * 2.0f + coin.phase) * 0.12f;

    const Vec3 d{playerPos.x - coin.position.x, playerPos.y - coin.position.y, playerPos.z - coin.position.z};
    const float dist = length3D(d);

    if (dist < coin.radius + BOARD_RADIUS)
    {
      g_player.score += 175;
      coin.active = false;
      setMessage("+175 Neon Coin", 1.0f);
    }
  }
}

void updateTimers(float dt)
{
  if (g_player.invuln > 0.0f)
  {
    g_player.invuln = std::max(0.0f, g_player.invuln - dt);
  }

  if (g_messageTimer > 0.0f)
  {
    g_messageTimer -= dt;
    if (g_messageTimer <= 0.0f)
    {
      g_message.clear();
    }
  }

  if (g_flash > 0.0f)
  {
    g_flash = std::max(0.0f, g_flash - dt);
  }
}

void drawRect2D(float x0, float y0, float x1, float y1, float r, float g, float b, float a)
{
  glColor4f(r, g, b, a);
  glBegin(GL_QUADS);
  glVertex2f(x0, y0);
  glVertex2f(x1, y0);
  glVertex2f(x1, y1);
  glVertex2f(x0, y1);
  glEnd();
}

void drawSkyGradient(float intensity)
{
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_LIGHTING);

  const float glow = std::clamp(0.0f + HORIZON_GLOW * intensity, 0.0f, 1.0f);
  const float topR = SKY_R1;
  const float topG = SKY_G1;
  const float topB = SKY_B1;
  const float horizonR = std::min(1.0f, SKY_R2 + glow * 0.2f);
  const float horizonG = std::min(1.0f, SKY_G2 + glow * 0.2f);
  const float horizonB = std::min(1.0f, SKY_B2 + glow * 0.2f);

  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glLoadIdentity();
  gluOrtho2D(0.0, static_cast<double>(g_windowW), 0.0, static_cast<double>(g_windowH));

  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glLoadIdentity();

  glBegin(GL_QUADS);
  glColor3f(topR, topG, topB);
  glVertex2f(0.0f, static_cast<float>(g_windowH));
  glVertex2f(static_cast<float>(g_windowW), static_cast<float>(g_windowH));
  glColor3f(horizonR, horizonG, horizonB);
  glVertex2f(static_cast<float>(g_windowW), 0.0f);
  glVertex2f(0.0f, 0.0f);
  glEnd();

  glMatrixMode(GL_PROJECTION);
  glPopMatrix();
  glMatrixMode(GL_MODELVIEW);
  glPopMatrix();

  glEnable(GL_LIGHTING);
  glEnable(GL_DEPTH_TEST);
}

void drawHdriSkyDome(const Vec3& center)
{
  if (!g_skyHdriTextureId)
  {
    return;
  }

  glDisable(GL_LIGHTING);
  glDisable(GL_FOG);
  glDisable(GL_CULL_FACE);
  glDepthMask(GL_FALSE);

  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, g_skyHdriTextureId);
  glColor3f(1.0f, 1.0f, 1.0f);

  glPushMatrix();
  glTranslatef(center.x, center.y, center.z);
  glRotatef(nowSeconds() * 0.65f, 0.0f, 1.0f, 0.0f);
  glScalef(-1.0f, 1.0f, 1.0f);

  GLUquadric* q = gluNewQuadric();
  gluQuadricTexture(q, GL_TRUE);
  gluQuadricNormals(q, GLU_SMOOTH);
  gluSphere(q, 920.0, 54, 34);
  gluDeleteQuadric(q);
  glPopMatrix();

  glDepthMask(GL_TRUE);
  glEnable(GL_CULL_FACE);
  glEnable(GL_FOG);
  glEnable(GL_LIGHTING);
}

bool beginHdriReflection(float mix)
{
  if (!g_skyHdriTextureId || mix <= 0.0f)
  {
    return false;
  }

  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, g_skyHdriTextureId);
  glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
  glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_SPHERE_MAP);
  glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_SPHERE_MAP);
  glEnable(GL_TEXTURE_GEN_S);
  glEnable(GL_TEXTURE_GEN_T);
  glColor4f(0.90f + mix * 0.08f, 0.90f + mix * 0.08f, 0.94f + mix * 0.06f, 1.0f);
  return true;
}

void endHdriReflection()
{
  glDisable(GL_TEXTURE_GEN_S);
  glDisable(GL_TEXTURE_GEN_T);
}

void drawSolidCube(float s)
{
  const float h = s * 0.5f;
  const GLfloat v[8][3] = {
    {-h, -h, -h},
    { h, -h, -h},
    { h,  h, -h},
    {-h,  h, -h},
    {-h, -h,  h},
    { h, -h,  h},
    { h,  h,  h},
    {-h,  h,  h},
  };

  const int faces[6][4] = {
    {0, 1, 2, 3},
    {4, 5, 6, 7},
    {0, 4, 7, 3},
    {1, 5, 6, 2},
    {3, 2, 6, 7},
    {0, 1, 5, 4},
  };
  const GLfloat n[6][3] = {
    {0.0f, 0.0f, -1.0f},
    {0.0f, 0.0f, 1.0f},
    {-1.0f, 0.0f, 0.0f},
    {1.0f, 0.0f, 0.0f},
    {0.0f, 1.0f, 0.0f},
    {0.0f, -1.0f, 0.0f},
  };

  for (int i = 0; i < 6; ++i)
  {
    glNormal3f(n[i][0], n[i][1], n[i][2]);
    glBegin(GL_QUADS);
    for (int j = 0; j < 4; ++j)
    {
      const auto& p = v[faces[i][j]];
      glVertex3f(p[0], p[1], p[2]);
    }
    glEnd();
  }
}

void drawSolidTorus(float innerRadius, float outerRadius, int nsides, int rings)
{
  const float ringStep = TAU / static_cast<float>(rings);
  const float sideStep = TAU / static_cast<float>(nsides);

  glBegin(GL_QUAD_STRIP);
  for (int i = 0; i <= rings; ++i)
  {
    const float a0 = i * ringStep;
    const float a1 = (i + 1) * ringStep;
    const float nx0 = std::cos(a0), nz0 = std::sin(a0);
    const float nx1 = std::cos(a1), nz1 = std::sin(a1);

    for (int j = 0; j <= nsides; ++j)
    {
      const float b = j * sideStep;
      const float c = std::cos(b);
      const float s = std::sin(b);

      const float px0 = (outerRadius + innerRadius * c) * nx0;
      const float pz0 = (outerRadius + innerRadius * c) * nz0;
      const float py0 = innerRadius * s;
      const float vx0 = c * nx0;
      const float vz0 = c * nz0;
      const float vy0 = s;
      glNormal3f(vx0, vy0, vz0);
      glVertex3f(px0, py0, pz0);

      const float px1 = (outerRadius + innerRadius * c) * nx1;
      const float pz1 = (outerRadius + innerRadius * c) * nz1;
      const float py1 = innerRadius * s;
      const float vx1 = c * nx1;
      const float vz1 = c * nz1;
      const float vy1 = s;
      glNormal3f(vx1, vy1, vz1);
      glVertex3f(px1, py1, pz1);
    }
  }
  glEnd();
}

void drawSolidSphere(float r, int slices, int stacks)
{
  GLUquadric* q = gluNewQuadric();
  gluQuadricNormals(q, GLU_SMOOTH);
  gluSphere(q, r, slices, stacks);
  gluDeleteQuadric(q);
}

void drawCylinder(float radius, float height, int slices = 14, int stacks = 1)
{
  GLUquadric* q = gluNewQuadric();
  gluQuadricNormals(q, GLU_SMOOTH);

  gluCylinder(q, radius, radius, height, slices, stacks);
  glPushMatrix();
  glRotatef(180.0f, 0.0f, 1.0f, 0.0f);
  gluDisk(q, 0.0f, radius, slices, stacks);
  glPopMatrix();

  glPushMatrix();
  glTranslatef(0.0f, 0.0f, height);
  gluDisk(q, 0.0f, radius, slices, stacks);
  glPopMatrix();

  gluDeleteQuadric(q);
}

void drawTruck(float z)
{
  glPushMatrix();
  const float plateSpan = 0.90f;
  const float plateDrop = 0.085f;
  const float hangHeight = 0.24f;

  glTranslatef(0.0f, plateDrop, z);

  const GLfloat deckSteelAmbient[] = {0.19f, 0.20f, 0.22f, 1.0f};
  const GLfloat deckSteelDiffuse[] = {0.35f, 0.37f, 0.40f, 1.0f};
  const GLfloat deckSteelSpec[] = {0.55f, 0.58f, 0.62f, 1.0f};
  glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, deckSteelAmbient);
  glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, deckSteelDiffuse);
  glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, deckSteelSpec);
  glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 36.0f);
  const bool reflectionActive = beginHdriReflection(0.82f);

  glPushMatrix();
  glTranslatef(0.0f, -0.06f, 0.0f);
  glScalef(plateSpan, 0.14f, 0.38f);
  glColor3f(0.40f, 0.42f, 0.46f);
  drawSolidCube(1.0f);
  glPopMatrix();

  const GLfloat steelDarkAmbient[] = {0.14f, 0.14f, 0.16f, 1.0f};
  const GLfloat steelDarkDiffuse[] = {0.26f, 0.26f, 0.28f, 1.0f};
  const GLfloat bushingAmbient[] = {0.20f, 0.20f, 0.20f, 1.0f};
  const GLfloat bushingDiffuse[] = {0.08f, 0.08f, 0.09f, 1.0f};
  const GLfloat bushingSpec[] = {0.26f, 0.26f, 0.30f, 1.0f};
  const GLfloat hubAmbient[] = {0.08f, 0.08f, 0.10f, 1.0f};
  const GLfloat hubDiffuse[] = {0.14f, 0.14f, 0.16f, 1.0f};
  const GLfloat hubSpec[] = {0.40f, 0.40f, 0.44f, 1.0f};

  glPushMatrix();
  glTranslatef(0.0f, hangHeight * 0.15f, 0.0f);
  glScalef(0.20f, hangHeight, 0.13f);
  glColor3f(0.22f, 0.22f, 0.24f);
  drawSolidCube(1.0f);
  glPopMatrix();

  glPushMatrix();
  glTranslatef(0.0f, 0.04f, 0.0f);
  glScalef(0.30f, 0.14f, 0.11f);
  glColor3f(0.16f, 0.16f, 0.19f);
  drawSolidCube(1.0f);
  glPopMatrix();

  glPushMatrix();
  glTranslatef(0.0f, -0.02f, 0.0f);
  glRotatef(90.0f, 1.0f, 0.0f, 0.0f);
  glColor3f(0.75f, 0.77f, 0.80f);
  drawCylinder(0.04f, 0.48f, 18, 6);
  glPopMatrix();

  for (float x : {-0.36f, 0.36f})
  {
    glPushMatrix();
    glTranslatef(x, -0.14f, 0.0f);
    glScalef(0.10f, 0.26f, 0.07f);
    glColor3f(0.30f, 0.32f, 0.34f);
    drawSolidCube(1.0f);
    glPopMatrix();
  }

  glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, bushingAmbient);
  glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, bushingDiffuse);
  glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, bushingSpec);
  glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 16.0f);

  glPushMatrix();
  glTranslatef(0.0f, -0.02f, 0.0f);
  glRotatef(90.0f, 1.0f, 0.0f, 0.0f);
  glColor3f(0.15f, 0.16f, 0.18f);
  drawCylinder(0.048f, 0.12f, 18, 6);
  glPopMatrix();

  glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, steelDarkAmbient);
  glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, steelDarkDiffuse);
  glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 28.0f);
  glPushMatrix();
  glTranslatef(0.0f, -0.03f, -0.15f);
  glRotatef(90.0f, 0.0f, 1.0f, 0.0f);
  drawCylinder(0.014f, 0.30f, 14, 4);
  glPopMatrix();

  glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, hubAmbient);
  glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, hubDiffuse);
  glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, hubSpec);
  glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 16.0f);
  for (float zPos : {-0.13f, 0.13f})
  {
    for (float x : {-0.26f, 0.26f})
    {
      glPushMatrix();
      glTranslatef(x, -0.12f, zPos);
      glRotatef(90.0f, 0.0f, 1.0f, 0.0f);
      drawCylinder(0.022f, 0.006f, 16, 4);
      drawCylinder(0.015f, 0.008f, 16, 4);
      glPopMatrix();
    }
  }

  glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, steelDarkAmbient);
  glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, steelDarkDiffuse);
  glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 40.0f);
  for (float x : {-0.34f, 0.34f})
  {
    for (float zPos : {-0.13f, 0.13f})
    {
      glPushMatrix();
      glTranslatef(x, -0.22f, zPos);
      glRotatef(90.0f, 1.0f, 0.0f, 0.0f);
      glColor3f(0.23f, 0.23f, 0.26f);
      drawCylinder(0.008f, 0.030f, 12, 4);
      glPopMatrix();
    }
  }

  if (reflectionActive)
  {
    endHdriReflection();
  }
  glPopMatrix();
}

void drawDeck(float flex)
{
  const float safeFlex = std::clamp(flex, -BOARD_FLEX_LIMIT, 0.0f);
  const float deckHalfLen = kProceduralDeckLength * 0.5f;
  const float widthScale = std::clamp(1.0f + safeFlex * 0.35f, 0.88f, 1.0f);

  const auto deckHalfWidth = [&](float zNorm)
  {
    const float ta = std::clamp(std::fabs(zNorm), 0.0f, 1.0f);
    const float taper = 1.0f - ta;
    return (kProceduralDeckNoseWidth * 0.5f + (kProceduralDeckWidth * 0.5f - kProceduralDeckNoseWidth * 0.5f) * std::pow(taper, 1.22f))
      * widthScale;
  };

  const auto deckTopProfile = [&](float xNorm, float zNorm)
  {
    const float absX = std::clamp(std::fabs(xNorm), 0.0f, 1.0f);
    const float absZ = std::clamp(std::fabs(zNorm), 0.0f, 1.0f);
    const float concave = -0.023f * (1.0f - std::pow(absX, 1.9f));
    const float kick = 0.044f * std::pow(std::max(0.0f, absZ - 0.18f) / 0.82f, 2.1f);
    const float flexProfile = safeFlex * (0.55f + 0.45f * (1.0f - absZ));
    return 0.102f + concave + kick + flexProfile;
  };

  const auto deckBottomProfile = [&](float xNorm, float zNorm)
  {
    const float absX = std::clamp(std::fabs(xNorm), 0.0f, 1.0f);
    const float absZ = std::clamp(std::fabs(zNorm), 0.0f, 1.0f);
    const float profile = 0.012f * (1.0f - absX);
    const float nose = 0.008f * std::pow(std::max(0.0f, absZ - 0.16f) / 0.84f, 1.9f);
    return -0.038f - profile - nose + safeFlex * 0.5f;
  };

  const GLfloat deckAmbient[] = {0.26f, 0.14f, 0.08f, 1.0f};
  const GLfloat deckDiffuse[] = {0.70f, 0.52f, 0.34f, 1.0f};
  const GLfloat deckSpec[] = {0.20f, 0.16f, 0.13f, 1.0f};
  const GLfloat deckMetallicSpec[] = {0.34f, 0.34f, 0.38f, 1.0f};
  const GLfloat gripAmbient[] = {0.05f, 0.05f, 0.05f, 1.0f};
  const GLfloat gripDiffuse[] = {0.06f, 0.06f, 0.07f, 1.0f};
  const GLfloat gripSpec[] = {0.45f, 0.45f, 0.50f, 1.0f};

  constexpr int DeckSlices = 24;

  glPushMatrix();
  glTranslatef(0.0f, 0.02f, 0.0f);
  glScalef(1.0f, 1.0f + safeFlex, 1.0f);

  glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, deckAmbient);
  glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, deckDiffuse);
  glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, deckSpec);
  glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 18.0f);

  for (int i = 0; i < DeckSlices; ++i)
  {
    const float t0 = (static_cast<float>(i) / static_cast<float>(DeckSlices)) * 2.0f - 1.0f;
    const float t1 = (static_cast<float>(i + 1) / static_cast<float>(DeckSlices)) * 2.0f - 1.0f;
    const float z0 = t0 * deckHalfLen;
    const float z1 = t1 * deckHalfLen;

    const float w0 = deckHalfWidth(t0);
    const float w1 = deckHalfWidth(t1);
    const float w0n = std::clamp(w0 / (kProceduralDeckWidth * 0.5f), 0.0f, 1.0f);
    const float w1n = std::clamp(w1 / (kProceduralDeckWidth * 0.5f), 0.0f, 1.0f);

    const float topT0L = deckTopProfile(-w0n, t0);
    const float topT0R = deckTopProfile(w0n, t0);
    const float topT1L = deckTopProfile(-w1n, t1);
    const float topT1R = deckTopProfile(w1n, t1);
    const float botT0L = deckBottomProfile(-w0n, t0);
    const float botT0R = deckBottomProfile(w0n, t0);
    const float botT1L = deckBottomProfile(-w1n, t1);
    const float botT1R = deckBottomProfile(w1n, t1);

    glBegin(GL_QUADS);
    glNormal3f(0.0f, 1.0f, 0.0f);
    glVertex3f(-w0, topT0L, z0);
    glVertex3f(w0, topT0R, z0);
    glVertex3f(w1, topT1R, z1);
    glVertex3f(-w1, topT1L, z1);

    glNormal3f(0.0f, -1.0f, 0.0f);
    glVertex3f(-w1, botT1L, z1);
    glVertex3f(w1, botT1R, z1);
    glVertex3f(w0, botT0R, z0);
    glVertex3f(-w0, botT0L, z0);
    glEnd();

    glBegin(GL_QUADS);
    glNormal3f(-1.0f, 0.0f, 0.0f);
    glVertex3f(-w0, topT0L, z0);
    glVertex3f(-w1, topT1L, z1);
    glVertex3f(-w1, botT1L, z1);
    glVertex3f(-w0, botT0L, z0);

    glNormal3f(1.0f, 0.0f, 0.0f);
    glVertex3f(w0, botT0R, z0);
    glVertex3f(w1, botT1R, z1);
    glVertex3f(w1, topT1R, z1);
    glVertex3f(w0, topT0R, z0);
    glEnd();
  }

  const float frontZ = deckHalfLen;
  const float backZ = -deckHalfLen;
  const float frontW = deckHalfWidth(1.0f);
  const float backW = deckHalfWidth(-1.0f);
  const float fWn = std::clamp(frontW / (kProceduralDeckWidth * 0.5f), 0.0f, 1.0f);
  const float bWn = std::clamp(backW / (kProceduralDeckWidth * 0.5f), 0.0f, 1.0f);
  const float frontTopL = deckTopProfile(-fWn, 1.0f);
  const float frontTopR = deckTopProfile(fWn, 1.0f);
  const float frontBotL = deckBottomProfile(-fWn, 1.0f);
  const float frontBotR = deckBottomProfile(fWn, 1.0f);
  const float backTopL = deckTopProfile(-bWn, -1.0f);
  const float backTopR = deckTopProfile(bWn, -1.0f);
  const float backBotL = deckBottomProfile(-bWn, -1.0f);
  const float backBotR = deckBottomProfile(bWn, -1.0f);

  glBegin(GL_QUADS);
  glNormal3f(0.0f, 0.0f, 1.0f);
  glVertex3f(-frontW, frontTopL, frontZ);
  glVertex3f(-frontW, frontBotL, frontZ);
  glVertex3f(frontW, frontBotR, frontZ);
  glVertex3f(frontW, frontTopR, frontZ);

  glNormal3f(0.0f, 0.0f, -1.0f);
  glVertex3f(-backW, backTopL, backZ);
  glVertex3f(backW, backTopR, backZ);
  glVertex3f(backW, backBotR, backZ);
  glVertex3f(-backW, backBotL, backZ);
  glEnd();

  glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, gripAmbient);
  glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, gripDiffuse);
  glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, gripSpec);
  glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 36.0f);

  const float gripInset = 0.03f;
  const float gripLift = 0.004f;
  glColor3f(0.04f, 0.04f, 0.05f);
  for (int i = 0; i < DeckSlices; ++i)
  {
    const float t0 = (static_cast<float>(i) / static_cast<float>(DeckSlices)) * 2.0f - 1.0f;
    const float t1 = (static_cast<float>(i + 1) / static_cast<float>(DeckSlices)) * 2.0f - 1.0f;
    const float z0 = t0 * deckHalfLen;
    const float z1 = t1 * deckHalfLen;

    const float w0 = deckHalfWidth(t0) - gripInset;
    const float w1 = deckHalfWidth(t1) - gripInset;
    const float w0n = std::clamp(w0 / (kProceduralDeckWidth * 0.5f), 0.0f, 1.0f);
    const float w1n = std::clamp(w1 / (kProceduralDeckWidth * 0.5f), 0.0f, 1.0f);

    const float top0L = deckTopProfile(-w0n, t0) + gripLift;
    const float top0R = deckTopProfile(w0n, t0) + gripLift;
    const float top1L = deckTopProfile(-w1n, t1) + gripLift;
    const float top1R = deckTopProfile(w1n, t1) + gripLift;
    const float bottom0 = deckTopProfile(-w0n, t0) - 0.006f;
    const float bottom1 = deckTopProfile(-w1n, t1) - 0.006f;

    glBegin(GL_QUADS);
    glNormal3f(0.0f, 1.0f, 0.0f);
    glVertex3f(-w0, top0L, z0);
    glVertex3f(w0, top0R, z0);
    glVertex3f(w1, top1R, z1);
    glVertex3f(-w1, top1L, z1);

    glNormal3f(0.0f, -1.0f, 0.0f);
    glVertex3f(-w1, bottom1, z1);
    glVertex3f(w1, bottom1, z1);
    glVertex3f(w0, bottom0, z0);
    glVertex3f(-w0, bottom0, z0);
    glEnd();
  }

  const float clampL = kProceduralDeckWidth * 0.54f;
  const float boltDepth = kProceduralDeckNoseWidth * 0.16f * widthScale;
  const float boltZ[4] = {-0.86f, -0.32f, 0.24f, 0.82f};
  const GLfloat clipAmbient[] = {0.16f, 0.16f, 0.18f, 1.0f};
  const GLfloat clipDiffuse[] = {0.27f, 0.27f, 0.30f, 1.0f};
  const GLfloat clipSpec[] = {0.45f, 0.45f, 0.50f, 1.0f};
  glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, clipAmbient);
  glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, clipDiffuse);
  glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, clipSpec);
  glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 18.0f);
  glColor3f(0.30f, 0.30f, 0.34f);
  for (float side : {-1.0f, 1.0f})
  {
    for (float z : boltZ)
    {
      const float x = side * clampL;
      const float t = z / deckHalfLen;
      const float tNorm = std::clamp(t, -1.0f, 1.0f);
      const float xNorm = std::clamp(x / (kProceduralDeckWidth * 0.5f), -1.0f, 1.0f);
      const float y = deckTopProfile(xNorm, tNorm) + 0.008f;
      glPushMatrix();
      glTranslatef(x, y, z);
      glRotatef(90.0f, 0.0f, 1.0f, 0.0f);
      drawCylinder(boltDepth, 0.022f, 10, 4);
      glPushMatrix();
      glTranslatef(0.0f, 0.0f, 0.022f);
      glColor3f(0.85f, 0.84f, 0.80f);
      drawCylinder(0.008f, 0.010f, 12, 4);
      glPopMatrix();
      glPopMatrix();
    }
  }

  const GLfloat railAmbient[] = {0.14f, 0.14f, 0.16f, 1.0f};
  const GLfloat railDiffuse[] = {0.24f, 0.24f, 0.27f, 1.0f};
  const GLfloat railSpec[] = {0.55f, 0.55f, 0.58f, 1.0f};
  glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, railAmbient);
  glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, railDiffuse);
  glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, railSpec);
  glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 22.0f);
  for (float x : {-0.30f, 0.30f})
  {
    for (float z : {-1.0f, -0.45f, 0.25f, 1.0f})
    {
      const float t = z / deckHalfLen;
      const float xNorm = std::clamp(x / (kProceduralDeckWidth * 0.5f), -1.0f, 1.0f);
      const float y = deckTopProfile(xNorm, t) + 0.022f;
      glPushMatrix();
      glTranslatef(x, y, z);
      glScalef(0.028f, 0.018f, 0.038f);
      drawSolidCube(1.0f);
      glPopMatrix();
    }
  }

  glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, deckMetallicSpec);
  glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, deckSpec);
  glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, deckMetallicSpec);
  glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 22.0f);
  for (float x : {-0.98f, 0.98f})
  {
    for (float z : {-0.84f, 0.88f})
    {
      const float xNorm = std::clamp(x / (kProceduralDeckWidth * 0.5f), -1.0f, 1.0f);
      const float zNorm = z / deckHalfLen;
      const float top = deckTopProfile(xNorm, zNorm) + 0.009f;
      glPushMatrix();
      glTranslatef(x, top, z);
      glScalef(0.018f, 0.020f, 0.026f);
      drawSolidCube(1.0f);
      glPopMatrix();
    }
  }

  glPopMatrix();
}

void drawRealWheel(float spin, float flex)
{
  glPushMatrix();
  const float safeFlex = std::clamp(flex, -BOARD_FLEX_LIMIT, 0.0f);
  const float widthScale = 1.0f + safeFlex * 0.55f;
  glScalef(1.0f, 1.0f, widthScale);
  glRotatef(spin, 0.0f, 0.0f, 1.0f);

  const GLfloat rubberAmbient[] = {0.05f, 0.05f, 0.05f, 1.0f};
  const GLfloat rubberDiffuse[] = {0.08f, 0.08f, 0.08f, 1.0f};
  const GLfloat rubberSpec[] = {0.12f, 0.12f, 0.12f, 1.0f};
  const GLfloat edgeAmbient[] = {0.23f, 0.23f, 0.24f, 1.0f};
  const GLfloat edgeDiffuse[] = {0.40f, 0.40f, 0.42f, 1.0f};
  const GLfloat edgeSpec[] = {0.60f, 0.60f, 0.65f, 1.0f};
  const GLfloat sidewallAmbient[] = {0.10f, 0.10f, 0.11f, 1.0f};
  const GLfloat sidewallDiffuse[] = {0.16f, 0.16f, 0.17f, 1.0f};
  const GLfloat hubAmbient[] = {0.09f, 0.09f, 0.10f, 1.0f};
  const GLfloat hubDiffuse[] = {0.22f, 0.22f, 0.24f, 1.0f};
  const GLfloat bearingAmbient[] = {0.76f, 0.77f, 0.80f, 1.0f};
  const GLfloat bearingDiffuse[] = {0.91f, 0.92f, 0.96f, 1.0f};
  const GLfloat bearingSpec[] = {0.80f, 0.80f, 0.85f, 1.0f};
  const GLfloat boltAmbient[] = {0.09f, 0.09f, 0.11f, 1.0f};
  const GLfloat boltDiffuse[] = {0.18f, 0.18f, 0.19f, 1.0f};

  glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, rubberAmbient);
  glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, rubberDiffuse);
  glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, rubberSpec);
  glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 5.0f);

  glColor3f(0.06f, 0.06f, 0.08f);
  drawSolidTorus(0.050f, kProceduralWheelRadius, 22, 44);
  const bool reflectionActive = beginHdriReflection(0.65f);

  glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, edgeAmbient);
  glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, edgeDiffuse);
  glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, edgeSpec);
  glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 24.0f);

  glPushMatrix();
  glTranslatef(0.0f, 0.0f, -0.050f);
  glScalef(1.0f, 1.0f, 1.12f);
  drawCylinder(kProceduralWheelRadius * 0.93f, 0.013f, 24, 8);
  glPopMatrix();

  glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, sidewallAmbient);
  glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, sidewallDiffuse);
  glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, edgeSpec);

  glColor3f(0.36f, 0.36f, 0.40f);
  glPushMatrix();
  glTranslatef(0.0f, 0.0f, -0.050f);
  drawSolidTorus(0.018f, 0.14f, 16, 24);
  glPopMatrix();

  glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, edgeAmbient);
  glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, edgeDiffuse);
  glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, edgeSpec);
  glPushMatrix();
  glTranslatef(0.0f, 0.0f, -0.050f);
  for (int i = 0; i < 12; ++i)
  {
    glPushMatrix();
    glRotatef(static_cast<float>(i) * 30.0f, 0.0f, 0.0f, 1.0f);
    glTranslatef(0.22f, 0.0f, 0.0f);
    glScalef(0.020f, 0.030f, 0.025f);
    drawSolidCube(1.0f);
    glPopMatrix();
  }
  glPopMatrix();

  for (float side : {-0.065f, 0.065f})
  {
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, bearingAmbient);
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, bearingDiffuse);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, bearingSpec);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 75.0f);

    glPushMatrix();
    glTranslatef(0.0f, 0.0f, side);
    glPushMatrix();
    glTranslatef(0.0f, 0.0f, -0.018f);
    drawSolidTorus(0.028f, kProceduralWheelRadius * 0.47f, 16, 20);
    glPopMatrix();
    glRotatef(90.0f, 1.0f, 0.0f, 0.0f);
    drawCylinder(0.020f, 0.015f, 16, 6);
    glPopMatrix();
  }

  glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, hubAmbient);
  glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, hubDiffuse);
  glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, edgeSpec);
  glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 28.0f);

  glPushMatrix();
  glTranslatef(0.0f, 0.0f, -0.018f);
  glColor3f(0.20f, 0.20f, 0.21f);
  drawCylinder(0.075f, 0.036f, 20, 8);
  glPopMatrix();

  glPushMatrix();
  glTranslatef(0.0f, 0.0f, 0.0f);
  drawCylinder(0.072f, 0.006f, 20, 8);
  glColor3f(0.16f, 0.16f, 0.17f);
  drawCylinder(0.075f, 0.004f, 20, 8);
  glPopMatrix();

  glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, hubDiffuse);
  glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, bearingDiffuse);
  for (float side : {-1.0f, 1.0f})
  {
    for (int i = 0; i < 4; ++i)
    {
      const float angle = static_cast<float>(i) * 90.0f;
      glPushMatrix();
      glRotatef(angle, 0.0f, 0.0f, 1.0f);
      glTranslatef(0.165f * side, 0.0f, 0.0f);
      glRotatef(90.0f, 0.0f, 0.0f, 1.0f);
      glScalef(0.022f, 0.020f, 0.008f);
      drawSolidCube(1.0f);
      glPopMatrix();
    }
  }

  glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, boltAmbient);
  glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, boltDiffuse);
  glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, sidewallAmbient);
  glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 20.0f);

  for (int i = 0; i < 6; ++i)
  {
    const float a = static_cast<float>(i) * 60.0f;
    glPushMatrix();
    glRotatef(a, 0.0f, 0.0f, 1.0f);
    glTranslatef(0.18f, 0.0f, -0.005f);
    glScalef(0.010f, 0.014f, 0.012f);
    drawSolidCube(1.0f);
    glPopMatrix();
  }

  glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, hubAmbient);
  glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, hubDiffuse);
  glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, edgeSpec);
  glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 22.0f);

  glPushMatrix();
  glTranslatef(0.0f, 0.0f, -0.022f);
  drawCylinder(0.075f, 0.030f, 20, 8);
  glPopMatrix();

  glPushMatrix();
  glTranslatef(0.0f, 0.0f, 0.0f);
  drawSolidSphere(0.055f, 12, 8);
  glPopMatrix();

  if (reflectionActive)
  {
    endHdriReflection();
  }
  glPopMatrix();
}

void drawGroundSegment(float startZ)
{
  const float halfW = BOARD_HALF_WIDTH;
  const float stepX = (halfW * 2.0f) / static_cast<float>(GROUND_SAMPLES);
  const float stepZ = SEGMENT_LENGTH / static_cast<float>(GROUND_SAMPLES);
  const float ambientPulse = 0.5f + 0.5f * std::sin(nowSeconds() * 1.1f + startZ * 0.02f);
  const float baseR = 0.13f + ambientPulse * 0.03f;
  const float baseG = 0.48f + ambientPulse * 0.03f;
  const float baseB = 0.41f + ambientPulse * 0.02f;

  for (int zi = 0; zi < GROUND_SAMPLES; ++zi)
  {
    const float z0 = startZ + zi * stepZ;
    const float z1 = z0 + stepZ;
    const int stripeRow = static_cast<int>(std::floor((z0 + z1) * 0.5f * 0.12f));
    const bool evenRow = (stripeRow & 1) == 0;

    glBegin(GL_TRIANGLE_STRIP);
    for (int xi = 0; xi <= GROUND_SAMPLES; ++xi)
    {
      const float x = -halfW + xi * stepX;
      const float y0 = terrainHeight(x, z0);
      const float y1 = terrainHeight(x, z1);

      const Vec3 n0 = terrainNormal(x, z0);
      const Vec3 n1 = terrainNormal(x, z1);
      const int sx = static_cast<int>(std::floor((x + halfW) * 0.25f));
      const bool evenCell = ((sx + stripeRow) & 1) == 0;
      const float accent = evenRow ? (evenCell ? 0.02f : -0.02f) : (evenCell ? -0.01f : 0.01f);

      glColor3f(baseR + accent, baseG + accent * 0.7f, baseB + accent * 0.9f);
      glNormal3f(n0.x, n0.y, n0.z);
      glVertex3f(x, y0, z0);
      glColor3f(baseR - accent, baseG - accent * 0.6f, baseB - accent * 0.8f);
      glNormal3f(n1.x, n1.y, n1.z);
      glVertex3f(x, y1, z1);
    }
    glEnd();
  }

  glDisable(GL_LIGHTING);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  glColor4f(0.0f, 0.0f, 0.0f, 0.12f);
  for (int zi = 0; zi <= GROUND_SAMPLES; zi += 6)
  {
    const float z = startZ + zi * stepZ;
    glBegin(GL_LINE_STRIP);
    for (int xi = 0; xi <= GROUND_SAMPLES; xi += 2)
    {
      const float x = -halfW + xi * stepX;
      glVertex3f(x, terrainHeight(x, z) + 0.03f, z);
    }
    glEnd();
  }

  for (int xi = 0; xi <= GROUND_SAMPLES; xi += 5)
  {
    const float x = -halfW + xi * stepX;
    glBegin(GL_LINE_STRIP);
    for (int zi = 0; zi <= GROUND_SAMPLES; ++zi)
    {
      const float z = startZ + zi * stepZ;
      glVertex3f(x, terrainHeight(x, z) + 0.03f, z);
    }
    glEnd();
  }

  glDisable(GL_BLEND);
  glEnable(GL_LIGHTING);
}

void drawWorld()
{
  const int centerSegment = static_cast<int>(std::floor(g_player.position.z / SEGMENT_LENGTH));
  for (int i = -2; i < 4; ++i)
  {
    const float z0 = static_cast<float>(centerSegment + i) * SEGMENT_LENGTH;
    drawGroundSegment(z0);
  }

  if (g_environmentModel.loaded && !g_environmentModelFallback)
  {
    constexpr float kEnvSpan = 120.0f;
    const float anchor = std::round(g_player.position.z / kEnvSpan) * kEnvSpan;
    const float y = terrainHeight(0.0f, anchor) - 0.05f;
    glPushMatrix();
    glTranslatef(0.0f, y, anchor);
    glColor3f(0.74f, 0.77f, 0.80f);
    drawObjModel(g_environmentModel, nullptr, false);
    glPopMatrix();
  }
}

void drawObstacle(const Obstacle& o)
{
  if (!o.active)
  {
    return;
  }

  glPushMatrix();
  glTranslatef(o.position.x, o.position.y, o.position.z);

  if (o.rail)
  {
    glColor3f(0.29f, 0.84f, 1.0f);
    glScalef(0.6f, 0.28f, 3.6f);
    drawSolidCube(1.0f);

    glPushMatrix();
    glTranslatef(0.0f, 0.55f, 0.0f);
    glScalef(2.8f, 0.18f, 0.18f);
    glColor3f(0.8f, 0.95f, 1.0f);
    drawSolidCube(1.0f);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(0.0f, 1.1f, 1.2f);
    glScalef(0.05f, 0.6f, 0.05f);
    glColor3f(0.6f, 0.72f, 0.9f);
    drawSolidCube(1.0f);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(0.0f, 1.1f, -1.2f);
    glScalef(0.05f, 0.6f, 0.05f);
    drawSolidCube(1.0f);
    glPopMatrix();
  }
  else
  {
    glColor3f(0.96f, 0.31f, 0.31f);
    glScalef(1.5f, 1.2f, 1.2f);
    drawSolidCube(1.0f);

    glPushMatrix();
    glTranslatef(0.0f, 0.7f, 0.0f);
    glScalef(1.0f, 0.12f, 1.0f);
    glColor3f(0.98f, 0.76f, 0.36f);
    drawSolidCube(1.0f);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(0.0f, -0.3f, 0.0f);
    glScalef(1.05f, 0.22f, 1.05f);
    glColor3f(0.12f, 0.12f, 0.16f);
    drawSolidCube(1.0f);
    glPopMatrix();
  }

  glPopMatrix();
}

void drawCoin(Coin& c)
{
  if (!c.active)
  {
    return;
  }

  glPushMatrix();
  glTranslatef(c.position.x, c.position.y, c.position.z);
  glRotatef(c.spin * 45.0f, 1.0f, 0.0f, 0.0f);
  glRotatef(c.spin * 20.0f, 0.0f, 1.0f, 0.0f);
  glColor3f(0.98f, 0.95f, 0.6f);
  drawSolidTorus(0.10f, 0.45f, 10, 28);

  glPushMatrix();
  glScalef(0.7f, 0.7f, 0.7f);
  glRotatef(c.spin * 160.0f, 0.0f, 1.0f, 0.0f);
  glColor4f(0.98f, 1.0f, 0.77f, 0.55f);
  drawSolidTorus(0.05f, 0.18f, 8, 16);
  glPopMatrix();

  glPopMatrix();
  glPopMatrix();
}

void drawPlayer()
{
  glPushMatrix();
  glTranslatef(g_player.position.x, g_player.position.y, g_player.position.z);
  glRotatef(g_player.yaw * DEG, 0.0f, 1.0f, 0.0f);

  const Vec3 groundN = g_playerGround.normal;
  const float pitch = std::asin(std::clamp(-groundN.y, -1.0f, 1.0f)) * DEG * -0.2f;
  const float groundSpeed = std::clamp(length2D(g_player.velocity), 0.0f, MAX_SPEED);
  const float wheelDir = (g_player.velocity.z >= 0.0f) ? 1.0f : -1.0f;
  const float wheelSpin = nowSeconds() * groundSpeed * 360.0f / (TAU * 0.22f) * wheelDir;
  const float roll = -std::asin(std::clamp(groundN.x, -1.0f, 1.0f)) * DEG * 0.35f;
  const float lean =
    (groundSpeed > 0.8f ? std::clamp((g_player.velocity.x / (groundSpeed + 0.001f)) * (g_player.velocity.z >= 0.0f ? 1.0f : -1.0f), -1.0f, 1.0f) * 16.0f : 0.0f);
  const float ollie = olliePopEnvelope(g_player.ollieAnim);
  const float riderTuck = OLLIE_RIDER_TUCK * ollie;
  float flipBoardRoll = 0.0f;
  constexpr float BOARD_NEUTRAL_FIX = 180.0f;

  if (g_player.trickState == TrickState::FlipTrick)
  {
    const FlipTrickType activeFlip = g_player.trickFsm.activeFlip();
    if (activeFlip == FlipTrickType::Kickflip || activeFlip == FlipTrickType::Heelflip)
    {
      const float flipProgress = std::clamp(g_player.trickFsm.stateTimer() / 0.65f, 0.0f, 1.0f);
      const float direction = activeFlip == FlipTrickType::Kickflip ? 1.0f : -1.0f;
      flipBoardRoll = 360.0f * flipProgress * direction;
    }
  }

  glRotatef(pitch, 1.0f, 0.0f, 0.0f);
  glRotatef(roll + lean, 0.0f, 0.0f, 1.0f);
  glRotatef(OLLIE_BOARD_PITCH * ollie, 1.0f, 0.0f, 0.0f);
  glTranslatef(0.0f, -0.22f + OLLIE_POP_LIFT * 0.45f * ollie, 0.0f);
  glPushMatrix();
  glRotatef(BOARD_NEUTRAL_FIX, 1.0f, 0.0f, 0.0f);
  glRotatef(flipBoardRoll, 0.0f, 0.0f, 1.0f);
  const float bounce = -0.03f * std::sin(nowSeconds() * 7.0f + g_player.position.z * 0.05f);

  const float deckFlex = std::clamp(g_player.deckFlex, -BOARD_FLEX_LIMIT, 0.0f);
  const float deckHeight = 0.02f + bounce + deckFlex * 0.9f + OLLIE_POP_LIFT * 0.6f * ollie;
  const bool useProceduralBoard = kUseProceduralSkateboard;
  const float wheelZOffset = kProceduralDeckLength * 0.384f;
  const float truckZOffset = kProceduralDeckLength * 0.347f;
  const float deckOffsetY = deckHeight;
  const float truckY = deckOffsetY + kProceduralTruckY + deckFlex * 0.3f;
  const float wheelY = deckOffsetY + kProceduralWheelY + deckFlex * 0.6f;
  const float procWheelX = kProceduralWheelX;

  const bool hasDeckGroup = hasObjGroup(g_skateboardModel, "Deck") || hasObjGroup(g_skateboardModel, "deck");
  const char* deckGroup = hasObjGroup(g_skateboardModel, "Deck") ? "Deck" : "deck";
  const bool hasTruckFront = hasObjGroup(g_skateboardModel, "TruckFront");
  const bool hasTruckRear = hasObjGroup(g_skateboardModel, "TruckRear");
  const bool hasTruckGroup = hasObjGroup(g_skateboardModel, "Truck");
  const bool hasExplicitTrucks = hasTruckFront || hasTruckRear;
  const bool hasAnyTrucks = hasExplicitTrucks || hasTruckGroup;
  const bool hasWheelGroup = hasObjGroup(g_skateboardModel, "Wheel");
  const bool hasWheelFL = hasObjGroup(g_skateboardModel, "WheelFL");
  const bool hasWheelFR = hasObjGroup(g_skateboardModel, "WheelFR");
  const bool hasWheelRL = hasObjGroup(g_skateboardModel, "WheelRL");
  const bool hasWheelRR = hasObjGroup(g_skateboardModel, "WheelRR");
  const bool hasExplicitWheels = hasWheelFL || hasWheelFR || hasWheelRL || hasWheelRR;
  const bool hasAnyWheels = hasExplicitWheels || hasWheelGroup;

  const bool hasNamedPartGroups = hasDeckGroup || hasAnyTrucks || hasAnyWheels;
  const bool drawFullImportedBoard =
    !useProceduralBoard &&
    !g_skateboardModelFallback &&
    g_skateboardModel.loaded &&
    !hasNamedPartGroups;

  if (drawFullImportedBoard)
  {
    glPushMatrix();
    glTranslatef(0.0f, deckHeight, 0.0f);
    const float flexScaleY = std::clamp(1.0f + deckFlex * 0.5f, 0.92f, 1.0f);
    glScalef(1.0f, flexScaleY, 1.0f);
    drawObjModel(g_skateboardModel, nullptr, true);
    glPopMatrix();
  }
  else
  {
    glPushMatrix();
    glTranslatef(0.0f, deckHeight, 0.0f);
    if (!useProceduralBoard && !g_skateboardModelFallback && g_skateboardModel.loaded && hasDeckGroup)
    {
      drawObjModel(g_skateboardModel, deckGroup, true);
    }
    else
    {
      drawDeck(deckFlex);
    }
    glPopMatrix();

    const std::array<float, 2> truckZ = {truckZOffset, -truckZOffset};
    const std::array<float, 2> wheelZ = {wheelZOffset, -wheelZOffset};
    const float wheelX = useProceduralBoard ? procWheelX : 0.35f;

    auto drawObjTruckAtZ = [&](float z, const char* groupName)
    {
      glPushMatrix();
      glTranslatef(0.0f, useProceduralBoard ? truckY : (deckHeight - 0.24f), z);
      glColor3f(0.17f, 0.17f, 0.21f);
      drawObjModel(g_skateboardModel, groupName, false);
      glPopMatrix();
    };

    auto drawObjWheelAt = [&](float x, float z, const char* groupName)
    {
      glPushMatrix();
      glTranslatef(x, useProceduralBoard ? wheelY : (deckHeight - 0.62f), z);
      glRotatef(90.0f, 0.0f, 1.0f, 0.0f);
      glRotatef(wheelSpin, 0.0f, 0.0f, 1.0f);
      glColor3f(0.14f, 0.14f, 0.18f);
      drawObjModel(g_skateboardModel, groupName, false);
      glPopMatrix();
    };

    if (!useProceduralBoard && hasAnyTrucks && !g_skateboardModelFallback && g_skateboardModel.loaded)
    {
      if (hasTruckGroup && !hasExplicitTrucks)
      {
        for (const float z : truckZ)
        {
          drawObjTruckAtZ(z, "Truck");
        }
      }
      else
      {
        if (hasTruckFront)
        {
          drawObjTruckAtZ(truckZ[0], "TruckFront");
        }
        if (hasTruckRear)
        {
          drawObjTruckAtZ(truckZ[1], "TruckRear");
        }
      }
    }
    else
    {
      for (const float z : truckZ)
      {
        glPushMatrix();
        glTranslatef(0.0f, truckY, z);
        drawTruck(0.0f);
        glPopMatrix();
      }
    }

    if (!useProceduralBoard && hasAnyWheels && !g_skateboardModelFallback && g_skateboardModel.loaded)
    {
      if (hasWheelGroup && !hasExplicitWheels)
      {
        for (const float z : wheelZ)
        {
          for (float x : {-wheelX, wheelX})
          {
            drawObjWheelAt(x, z, "Wheel");
          }
        }
      }
      else
      {
        if (hasWheelFL)
        {
          drawObjWheelAt(-wheelX, wheelZ[0], "WheelFL");
        }
        if (hasWheelFR)
        {
          drawObjWheelAt(wheelX, wheelZ[0], "WheelFR");
        }
        if (hasWheelRL)
        {
          drawObjWheelAt(-wheelX, wheelZ[1], "WheelRL");
        }
        if (hasWheelRR)
        {
          drawObjWheelAt(wheelX, wheelZ[1], "WheelRR");
        }
      }
    }
    else
    {
      for (const float z : wheelZ)
      {
        glPushMatrix();
        for (float x : {-wheelX, wheelX})
        {
          glPushMatrix();
          glTranslatef(x, wheelY, z);
          glRotatef(90.0f, 0.0f, 1.0f, 0.0f);
          drawRealWheel(wheelSpin, deckFlex);
          glPopMatrix();
        }
        glPopMatrix();
      }
    }
  }

  glPopMatrix();

  // Rider high-fidelity asset (fallback to silhouette if missing).
  glPushMatrix();
  glTranslatef(0.0f, deckHeight + OLLIE_RIDER_RISE * ollie + 0.13f, 0.06f);
  glRotatef(-6.0f - riderTuck, 1.0f, 0.0f, 0.0f);
  glRotatef(riderTuck * 0.45f, 0.0f, 0.0f, 1.0f);
  if (g_characterModel.loaded && !g_characterModelFallback)
  {
    glScalef(0.62f, 0.62f, 0.62f);
    glColor3f(0.86f, 0.89f, 0.95f);
    drawObjModel(g_characterModel, nullptr, false);
  }
  else
  {
    glRotatef(-10.0f, 1.0f, 0.0f, 0.0f);

    // Left leg
    glPushMatrix();
    glTranslatef(-0.20f, 1.10f, 0.0f);
    glScalef(0.28f, 2.20f, 0.30f);
    glColor3f(0.08f, 0.08f, 0.14f);
    drawSolidCube(1.0f);
    glPopMatrix();

    // Right leg
    glPushMatrix();
    glTranslatef(0.20f, 1.10f, 0.0f);
    glScalef(0.28f, 2.20f, 0.30f);
    glColor3f(0.08f, 0.08f, 0.14f);
    drawSolidCube(1.0f);
    glPopMatrix();

    // Torso
    glPushMatrix();
    glTranslatef(0.0f, 3.05f, 0.04f);
    glScalef(0.94f, 1.70f, 0.60f);
    glColor3f(0.06f, 0.08f, 0.11f);
    drawSolidCube(1.0f);
    glPopMatrix();

    // Head
    glPushMatrix();
    glTranslatef(0.0f, 4.38f, 0.14f);
    glColor3f(0.98f, 0.95f, 0.85f);
    drawSolidSphere(0.42f, 12, 8);
    glPopMatrix();
  }
  glPopMatrix();

  glPopMatrix();
}

void drawSkidTrail(float speed)
{
  if (speed < 6.0f || !g_player.grounded)
  {
    return;
  }

  const float intensity = std::clamp((speed - 6.0f) / 12.0f, 0.0f, 1.0f);
  const int segments = 10;

  glPushMatrix();
  glTranslatef(g_player.position.x, g_player.position.y - BOARD_RADIUS * 0.2f, g_player.position.z);
  glRotatef(g_player.yaw * DEG, 0.0f, 1.0f, 0.0f);

  glDisable(GL_LIGHTING);
  glDisable(GL_DEPTH_TEST);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  for (int i = 0; i < segments; ++i)
  {
    const float t0 = static_cast<float>(i) / static_cast<float>(segments);
    const float t1 = static_cast<float>(i + 1) / static_cast<float>(segments);
    const float z0 = -t0 * 5.0f;
    const float z1 = -t1 * 5.0f;
    const float w0 = 0.3f + t0 * 0.9f;
    const float w1 = 0.3f + t1 * 1.2f;
    const float alpha = (1.0f - t0) * 0.26f * intensity;
    glColor4f(1.0f, 1.0f, 1.0f, alpha);
    glBegin(GL_QUAD_STRIP);
    glVertex3f(-w0, 0.0f, z0);
    glVertex3f(w0, 0.0f, z0);
    glVertex3f(-w1, 0.0f, z1);
    glVertex3f(w1, 0.0f, z1);
    glEnd();
  }

  glDisable(GL_BLEND);
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_LIGHTING);
  glPopMatrix();
}

void drawAgentOrchestrationStatus()
{
  const float left = static_cast<float>(g_windowW - 420);
  const float top = static_cast<float>(g_windowH - 10);
  const float bottom = static_cast<float>(g_windowH - 208);

  drawRect2D(left, bottom, static_cast<float>(g_windowW - 10), top, 0.03f, 0.08f, 0.14f, 0.56f);
  drawRect2D(left + 2.0f, bottom + 2.0f, static_cast<float>(g_windowW - 12), top - 2.0f, 0.06f, 0.16f, 0.22f, 0.38f);

  drawTextWithShadow(static_cast<int>(left + 10.0f), g_windowH - 26, "Agent Mesh: OBJ Render Orchestration", 0.97f);

  int lineY = g_windowH - 46;
  for (const auto& agent : g_agentOrchestrator.agents)
  {
    std::string status = std::string(agentRoleName(agent.role)) +
      (agent.online ? " [online]" : " [offline]") +
      "  " + std::to_string(static_cast<int>(agent.latencyMs)) + "ms";
    drawTextWithShadow(static_cast<int>(left + 10.0f), lineY, status, agent.online ? 0.92f : 0.65f);
    lineY -= 16;
    if (lineY < g_windowH - 132)
    {
      break;
    }
  }

  const size_t maxEvents = std::min<size_t>(4, g_agentOrchestrator.eventLog.size());
  lineY = g_windowH - 146;
  for (size_t i = 0; i < maxEvents; ++i)
  {
    const size_t idx = g_agentOrchestrator.eventLog.size() - 1 - i;
    drawTextWithShadow(static_cast<int>(left + 10.0f), lineY, g_agentOrchestrator.eventLog[idx], 0.85f);
    lineY -= 14;
  }
}

void drawManualBalanceOverlay()
{
  if (g_state != GameState::Play && g_state != GameState::Pause)
  {
    return;
  }

  const float panelW = 198.0f;
  const float panelH = 42.0f;
  const float panelX0 = static_cast<float>(g_windowW) - panelW - 12.0f;
  const float panelX1 = static_cast<float>(g_windowW) - 12.0f;
  const float panelY0 = static_cast<float>(g_windowH - 190);
  const float panelY1 = panelY0 + panelH;
  const float pad = 4.0f;

  drawRect2D(panelX0, panelY0, panelX1, panelY1, 0.03f, 0.08f, 0.14f, 0.45f);
  drawRect2D(panelX0 + pad, panelY0 + pad, panelX1 - pad, panelY1 - pad, 0.06f, 0.16f, 0.24f, 0.34f);

  const float barX0 = panelX0 + 12.0f;
  const float barX1 = panelX1 - 12.0f;
  const float barY0 = panelY0 + 8.0f;
  const float barY1 = panelY0 + 18.0f;
  const float center = 0.5f * (barX0 + barX1);
  const float level = std::clamp(0.5f + (g_player.manualBalance * 0.5f), 0.0f, 1.0f);
  const float knobX = barX0 + (barX1 - barX0) * level;
  const float drift = std::abs(g_player.manualBalance);
  const float stability = std::clamp(1.0f - drift, 0.0f, 1.0f);

  drawRect2D(barX0, barY0, barX1, barY1, 0.22f, 0.22f, 0.26f, 0.45f);
  drawRect2D(center - 1.0f, barY0 - 1.0f, center + 1.0f, barY1 + 1.0f, 0.92f, 0.90f, 0.72f, 0.90f);

  float indicatorR = 0.28f + 0.62f * stability;
  float indicatorG = 0.90f * stability + 0.25f * (1.0f - stability);
  float indicatorB = 0.28f;
  drawRect2D(knobX - 2.0f, barY0 - 2.0f, knobX + 2.0f, barY1 + 2.0f, indicatorR, indicatorG, indicatorB, 0.92f);

  drawTextWithShadow(static_cast<int>(panelX0 + 9.0f), static_cast<int>(panelY1 - 8.0f), "Manual Balance", 0.95f);

  char balanceText[48];
  if (drift > BALANCE_WARNING)
  {
    std::snprintf(balanceText, sizeof(balanceText), "Stability: %d%%  !", static_cast<int>(std::round(stability * 100.0f)));
  }
  else
  {
    std::snprintf(balanceText, sizeof(balanceText), "Stability: %d%%", static_cast<int>(std::round(stability * 100.0f)));
  }
  drawTextWithShadow(static_cast<int>(panelX0 + 9.0f), static_cast<int>(panelY0 + 24.0f), balanceText, 0.94f);
}

void drawHUD()
{
  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glLoadIdentity();
  gluOrtho2D(0.0, g_windowW, 0.0, g_windowH);

  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glLoadIdentity();

  glDisable(GL_DEPTH_TEST);
  glDisable(GL_LIGHTING);
  glDisable(GL_FOG);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  drawRect2D(8.0f, static_cast<float>(g_windowH - 170), static_cast<float>(220), static_cast<float>(g_windowH - 8), 0.03f, 0.07f, 0.12f, 0.55f);
  drawRect2D(10.0f, static_cast<float>(g_windowH - 168), 218.0f, static_cast<float>(g_windowH - 10), 0.06f, 0.16f, 0.24f, 0.40f);

  glColor3f(0.95f, 0.98f, 1.0f);
  char buf[128];

  const float speed = length2D(g_player.velocity);
  std::snprintf(buf, sizeof(buf), "Speed: %.1f", speed);
  drawTextWithShadow(14, g_windowH - 26, buf, 0.96f);

  std::snprintf(buf, sizeof(buf), "Score: %d", g_player.score);
  drawTextWithShadow(14, g_windowH - 44, buf, 0.96f);

  std::snprintf(buf, sizeof(buf), "Distance: %.0f", g_player.distance);
  drawTextWithShadow(14, g_windowH - 62, buf, 0.96f);

  std::snprintf(buf, sizeof(buf), "Time: %d", static_cast<int>(std::ceil(g_timeLeft)));
  drawTextWithShadow(14, g_windowH - 80, buf, 0.96f);

  std::snprintf(buf, sizeof(buf), "Lives: %d", g_player.lives);
  drawTextWithShadow(14, g_windowH - 98, buf, 0.96f);

  const std::string trickLabel = activeTrickLabel();
  std::snprintf(buf, sizeof(buf), "Trick: %s", trickLabel.c_str());
  drawTextWithShadow(14, g_windowH - 116, buf, 0.98f);

  if (!g_message.empty())
  {
    drawTextWithShadow(14, g_windowH - 134, g_message, 1.0f);
  }

  drawManualBalanceOverlay();
  drawAgentOrchestrationStatus();

  if (g_player.comboEngine.active())
  {
    char comboLine[96];
    char bankLine[96];
    std::snprintf(comboLine, sizeof(comboLine), "Multiplier x%.2f", g_player.comboEngine.multiplier());
    std::snprintf(bankLine, sizeof(bankLine), "Pending %d", g_player.comboEngine.pendingScore());

    const std::string comboTitle = "COMBO LIVE";
    const int centerX = g_windowW / 2;
    drawTextWithShadow(centerX - static_cast<int>(comboTitle.size()) * 4, g_windowH / 2 + 68, comboTitle, 1.0f);
    drawTextWithShadow(centerX - static_cast<int>(std::string(comboLine).size()) * 4, g_windowH / 2 + 50, comboLine, 0.98f);
    drawTextWithShadow(centerX - static_cast<int>(std::string(bankLine).size()) * 4, g_windowH / 2 + 32, bankLine, 0.98f);
  }

  const float pulse = 0.5f + 0.5f * g_pausePulse;

  if (g_state == GameState::Menu)
  {
    drawRect2D(
      12.0f,
      static_cast<float>(g_windowH - 250),
      static_cast<float>(g_windowW - 12),
      static_cast<float>(g_windowH - 12),
      0.03f, 0.12f, 0.22f,
      0.70f + pulse * 0.08f);
    drawRect2D(
      18.0f,
      static_cast<float>(g_windowH - 244),
      static_cast<float>(g_windowW - 18),
      static_cast<float>(g_windowH - 18),
      0.06f, 0.22f, 0.36f,
      0.20f);

    drawTextWithShadow(g_windowW / 2 - 162, g_windowH - 210, "ONESHOT SKATE - NEO CITY", 0.98f);

    drawTextWithShadow(g_windowW / 2 - 120, g_windowH - 186, "Native Desktop Prototype", 0.90f);
    drawTextWithShadow(g_windowW / 2 - 210, g_windowH - 164, "WASD / Arrows: Turn + Carve | Space: Ollie | P: Pause | R: Replay", 0.95f);
    drawTextWithShadow(g_windowW / 2 - 170, g_windowH - 146, "Press Enter to launch", 1.0f);
  }

  if (g_state == GameState::Over || g_state == GameState::Win)
  {
    drawRect2D(0.0f, 0.0f, static_cast<float>(g_windowW), static_cast<float>(g_windowH), 0.02f, 0.02f, 0.02f, 0.52f);
    const float cx = static_cast<float>(g_windowW) * 0.5f;
    drawTextWithShadow(static_cast<int>(cx - static_cast<float>(g_overlayTitle.size()) * 4.0f), g_windowH / 2 + 44, g_overlayTitle, 1.0f);
    drawTextWithShadow(static_cast<int>(cx - static_cast<float>(g_overlayHint.size()) * 4.0f), g_windowH / 2 + 18, g_overlayHint, 1.0f);
    drawRect2D(cx - 190.0f, g_windowH / 2 - 8, cx + 190.0f, g_windowH / 2 - 38, 0.16f, 0.12f, 0.05f, 0.55f);
  }

  if (g_state == GameState::Pause)
  {
    drawRect2D(0.0f, 0.0f, static_cast<float>(g_windowW), static_cast<float>(g_windowH), 0.0f, 0.08f, 0.05f, 0.30f + pulse * 0.15f);
    drawRect2D(
      static_cast<float>(g_windowW / 2 - 112),
      static_cast<float>(g_windowH / 2 + 4),
      static_cast<float>(g_windowW / 2 + 112),
      static_cast<float>(g_windowH / 2 - 52),
      0.06f,
      0.18f,
      0.07f,
      0.45f * pulse);
    drawTextWithShadow(g_windowW / 2 - 52, g_windowH / 2 + 30, "PAUSED", 1.0f);
    drawTextWithShadow(g_windowW / 2 - 140, g_windowH / 2 + 8, "Press Enter to continue", 0.95f);
  }

  glEnable(GL_DEPTH_TEST);
  glEnable(GL_LIGHTING);
  glEnable(GL_FOG);
  glDisable(GL_BLEND);

  glMatrixMode(GL_PROJECTION);
  glPopMatrix();
  glMatrixMode(GL_MODELVIEW);
  glPopMatrix();
}

void setFlashColor()
{
  if (g_flash > 0.0f)
  {
    const float damage = std::min(1.0f, g_flash * 1.8f);
    glClearColor(0.06f, 0.03f + damage * 0.14f, 0.05f, 1.0f);
  }
  else
  {
    glClearColor(0.06f, 0.12f, 0.25f, 1.0f);
  }
}

float activeCameraYaw()
{
  if (g_state == GameState::Menu)
  {
    return g_menuYaw;
  }

  if (g_state == GameState::Pause)
  {
    return g_player.yaw + std::sin(g_pausePulse * 2.0f) * 0.22f;
  }

  return g_player.yaw;
}

void setupSceneLighting()
{
  glEnable(GL_LIGHTING);
  glEnable(GL_LIGHT0);
  glEnable(GL_LIGHT1);
  glEnable(GL_COLOR_MATERIAL);
  glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);

  const float t = nowSeconds() * 0.1f;
  const float amb = g_skyHdriReady ? (0.36f + 0.03f * std::sin(t * 0.45f)) : (0.22f + 0.06f * std::sin(t * 0.7f));
  const GLfloat ambient[] = {
    amb * (g_skyHdriReady ? 0.62f : 0.50f),
    amb * (g_skyHdriReady ? 0.68f : 0.56f),
    amb * (g_skyHdriReady ? 0.78f : 0.65f),
    1.0f
  };
  glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambient);

  GLfloat lightPos0[] = { -8.0f + std::sin(t) * 2.0f, g_skyHdriReady ? 26.0f : 18.0f, 12.0f + std::cos(t * 0.8f) * 4.0f, 1.0f };
  GLfloat lightDiffuse0[] = {
    g_skyHdriReady ? 1.0f : 0.90f,
    g_skyHdriReady ? 0.98f : 0.93f,
    g_skyHdriReady ? 0.95f : 1.0f,
    1.0f
  };
  GLfloat lightSpecular0[] = {
    g_skyHdriReady ? 0.55f : 0.30f,
    g_skyHdriReady ? 0.54f : 0.33f,
    g_skyHdriReady ? 0.62f : 0.48f,
    1.0f
  };
  glLightfv(GL_LIGHT0, GL_POSITION, lightPos0);
  glLightfv(GL_LIGHT0, GL_DIFFUSE, lightDiffuse0);
  glLightfv(GL_LIGHT0, GL_SPECULAR, lightSpecular0);

  GLfloat lightPos1[] = { 4.0f, g_skyHdriReady ? 16.0f : 10.0f, -12.0f, 1.0f };
  GLfloat lightDiffuse1[] = {
    g_skyHdriReady ? 0.28f : 0.20f,
    g_skyHdriReady ? 0.30f : 0.18f,
    g_skyHdriReady ? 0.34f : 0.15f,
    1.0f
  };
  GLfloat lightSpecular1[] = {
    g_skyHdriReady ? 0.22f : 0.16f,
    g_skyHdriReady ? 0.24f : 0.14f,
    g_skyHdriReady ? 0.28f : 0.16f,
    1.0f
  };
  glLightfv(GL_LIGHT1, GL_POSITION, lightPos1);
  glLightfv(GL_LIGHT1, GL_DIFFUSE, lightDiffuse1);
  glLightfv(GL_LIGHT1, GL_SPECULAR, lightSpecular1);
}

void renderScene()
{
  setFlashColor();
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glEnable(GL_DEPTH_TEST);
  if (!g_skyHdriTextureId)
  {
    drawSkyGradient(1.0f - g_player.invuln * 0.3f);
  }

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluPerspective(60.0, static_cast<double>(g_windowW) / static_cast<double>(g_windowH), 0.1, 1600.0);

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  const float camYaw = activeCameraYaw();
  const float baseLookDistance = g_state == GameState::Menu ? 18.0f : 14.0f;
  const float speed = length2D(g_player.velocity);
  const float speedBias = std::min(0.36f, speed * 0.013f);
  const float lookDistance = baseLookDistance - speedBias * 2.0f;
  const float camY = g_player.position.y + (g_state == GameState::Menu ? 8.5f : 6.6f) - speedBias * 0.9f;
  const float camX = g_player.position.x - std::sin(camYaw) * lookDistance;
  const float camZ = g_player.position.z - std::cos(camYaw) * lookDistance;
  const float focusX = g_player.position.x + std::sin(camYaw) * 4.0f;
  const float focusZ = g_player.position.z + std::cos(camYaw) * 4.0f;
  const float focusY = g_player.position.y + (g_state == GameState::Menu ? 3.5f : 2.3f);
  const float shake = std::sin(nowSeconds() * 34.0f) * 0.06f * speedBias;
  const float lean = std::sin(nowSeconds() * 1.7f + g_player.yaw) * 0.025f * speedBias;
  gluLookAt(camX, camY, camZ,
            focusX,
            focusY + shake * 0.4f,
            focusZ,
            lean, 1.0f, 0.0f);

  if (g_skyHdriTextureId)
  {
    drawHdriSkyDome({camX, camY, camZ});
  }
  setupSceneLighting();

  drawWorld();

  for (const auto& o : g_world.obstacles)
  {
    drawObstacle(o);
  }

  for (auto& c : g_world.coins)
  {
    drawCoin(c);
  }

  drawSkidTrail(length2D(g_player.velocity));
  drawPlayer();

  drawHUD();

  SDL_GL_SwapWindow(g_window);
}

void tick()
{
  const float current = nowSeconds();
  g_pausePulse = std::sin(current * 6.5f);

  float frameDt = current - g_lastFrame;
  g_lastFrame = current;

  if (frameDt > 0.05f)
  {
    frameDt = 0.05f;
  }

  if (frameDt <= 0.0f)
  {
    frameDt = g_deltaBackup;
  }
  g_deltaBackup = frameDt;

  if (g_state == GameState::Menu)
  {
    g_menuYaw += frameDt * 0.5f;
    if (g_menuYaw > TAU)
    {
      g_menuYaw -= TAU;
    }
  }

  if (g_state == GameState::Play && !g_paused)
  {
    g_fixedPhysicsAccumulator = std::min(g_fixedPhysicsAccumulator + frameDt, MAX_PHYSICS_CATCHUP);

    while (g_fixedPhysicsAccumulator >= FIXED_PHYSICS_STEP)
    {
      const InputState input = buildInputState();
      refreshPlayerGroundState();
      const bool manualWipeout = updatePlayer(FIXED_PHYSICS_STEP, input);
      if (manualWipeout)
      {
        loseLife();
      }

      g_player.distance += length2D(g_player.velocity) * FIXED_PHYSICS_STEP;

      g_timeLeft -= FIXED_PHYSICS_STEP;
      if (g_timeLeft <= 0.0f)
      {
        winRun();
        g_timeLeft = 0.0f;
      }

      recycleWorld(FIXED_PHYSICS_STEP);
      updateObstaclesAndCoins(FIXED_PHYSICS_STEP);
      updateTrickState(FIXED_PHYSICS_STEP, input);
      updateTimers(FIXED_PHYSICS_STEP);

      g_fixedPhysicsAccumulator -= FIXED_PHYSICS_STEP;

      if (g_state != GameState::Play || g_paused)
      {
        break;
      }
    }

    g_jumpQueued = false;
  }
  else
  {
    g_fixedPhysicsAccumulator = 0.0f;
    updateTimers(frameDt);
  }
}

void setRunStateForKey(SDL_Keycode key)
{
  switch (key)
  {
    case ' ':
      g_jumpQueued = true;
      break;
    case 'p':
    case 'P':
      if (g_state == GameState::Play)
      {
        g_paused = !g_paused;
        g_state = g_paused ? GameState::Pause : GameState::Play;
      }
      break;
    case SDLK_RETURN:
    case SDLK_KP_ENTER:
      if (g_state == GameState::Menu)
      {
        resetRun();
      }
      else if (g_state == GameState::Pause)
      {
        g_state = GameState::Play;
        g_paused = false;
      }
      else if (g_state == GameState::Over || g_state == GameState::Win)
      {
        resetRun();
      }
      break;
    case 'r':
    case 'R':
      if (g_state == GameState::Over || g_state == GameState::Win)
      {
        resetRun();
      }
      break;
    default:
      break;
  }
}

void onKeyDown(SDL_Scancode scancode, SDL_Keycode key)
{
  if (scancode < g_keys.size())
  {
    g_keys[scancode] = true;
  }

  setRunStateForKey(key);
}

void onKeyUp(SDL_Scancode scancode, SDL_Keycode key)
{
  if (scancode < g_keys.size())
  {
    g_keys[scancode] = false;
  }
  if (key == SDLK_SPACE)
  {
    g_jumpQueued = false;
  }
}

void onEscapePressed()
{
  g_running = false;
}

void onReshape(int w, int h)
{
  g_windowW = std::max(1, w);
  g_windowH = std::max(1, h);
  glViewport(0, 0, g_windowW, g_windowH);
}

void setupGL()
{
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_CULL_FACE);
  glShadeModel(GL_SMOOTH);
  glEnable(GL_COLOR_MATERIAL);
  glEnable(GL_NORMALIZE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glClearColor(0.06f, 0.12f, 0.25f, 1.0f);

  if (!loadDefaultFont(18))
  {
    std::fprintf(stderr, "Warning: failed to load HUD font. HUD text may not render.\n");
  }

  glEnable(GL_FOG);
  glFogi(GL_FOG_MODE, GL_EXP2);
  GLfloat fogColor[] = { SKY_R1 * 0.7f, SKY_G1 * 0.8f, SKY_B1 * 0.9f, 1.0f };
  glFogfv(GL_FOG_COLOR, fogColor);
  glFogf(GL_FOG_DENSITY, 0.0125f);
  glHint(GL_FOG_HINT, GL_DONT_CARE);
  glFogf(GL_FOG_START, 45.0f);
  glFogf(GL_FOG_END, 700.0f);

  glEnable(GL_MULTISAMPLE);
  glEnable(GL_TEXTURE_2D);
}

void initGame()
{
  ensureSkateboardModelAssets();
  g_characterModelFallback = !ensureObjAsset(kCharacterObjPath, g_characterModel);
  g_environmentModelFallback = !ensureObjAsset(kEnvironmentObjPath, g_environmentModel);
  ensureHdriSkyTexture();

  g_agentOrchestrator.bootstrap();
  RenderJob startupJob;
  startupJob.objPath = kEnvironmentObjPath;
  startupJob.requiresPhysicsBake = true;
  startupJob.requiresRigging = false;
  startupJob.dispatchToRuntime = true;
  g_agentOrchestrator.submit(startupJob);

  makeWorld();
  resetRun();
  showOverlay("ONESHOT SKATE", "Press Enter to Start");
  g_state = GameState::Menu;
  g_lastFrame = nowSeconds();
}

void shutdownGameAssets()
{
  if (g_skateboardModel.textureId)
  {
    glDeleteTextures(1, &g_skateboardModel.textureId);
    g_skateboardModel.textureId = 0;
  }
  if (g_characterModel.textureId)
  {
    glDeleteTextures(1, &g_characterModel.textureId);
    g_characterModel.textureId = 0;
  }
  if (g_environmentModel.textureId)
  {
    glDeleteTextures(1, &g_environmentModel.textureId);
    g_environmentModel.textureId = 0;
  }
  if (g_skyHdriTextureId)
  {
    glDeleteTextures(1, &g_skyHdriTextureId);
    g_skyHdriTextureId = 0;
  }
}

void handleInput()
{
  SDL_Event event;

  while (SDL_PollEvent(&event))
  {
    switch (event.type)
    {
      case SDL_QUIT:
        g_running = false;
        break;
      case SDL_WINDOWEVENT:
        if (event.window.event == SDL_WINDOWEVENT_RESIZED)
        {
          onReshape(event.window.data1, event.window.data2);
        }
        break;
      case SDL_KEYDOWN:
      {
        const SDL_KeyboardEvent& e = event.key;
        if (e.repeat)
        {
          break;
        }

        if (e.keysym.sym == SDLK_ESCAPE)
        {
          onEscapePressed();
          break;
        }

        onKeyDown(e.keysym.scancode, e.keysym.sym);
        break;
      }
      case SDL_KEYUP:
      {
        const SDL_KeyboardEvent& e = event.key;
        onKeyUp(e.keysym.scancode, e.keysym.sym);
        break;
      }
      default:
        break;
    }
  }
}

} // namespace

int main(int argc, char** argv)
{
  if (SDL_Init(SDL_INIT_VIDEO) != 0)
  {
    std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
    return 1;
  }

  if (TTF_Init() != 0)
  {
    std::fprintf(stderr, "TTF_Init failed: %s\n", TTF_GetError());
    SDL_Quit();
    return 1;
  }

  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
  SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
  SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);

  g_window = SDL_CreateWindow(
    "OneShot Skate - Native C++",
    SDL_WINDOWPOS_CENTERED,
    SDL_WINDOWPOS_CENTERED,
    g_windowW,
    g_windowH,
    SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);

  if (!g_window)
  {
    std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
    TTF_Quit();
    SDL_Quit();
    return 1;
  }

  g_glContext = SDL_GL_CreateContext(g_window);
  if (!g_glContext)
  {
    std::fprintf(stderr, "SDL_GL_CreateContext failed: %s\n", SDL_GetError());
    SDL_DestroyWindow(g_window);
    TTF_Quit();
    SDL_Quit();
    return 1;
  }
  SDL_GL_SetSwapInterval(1);

  glViewport(0, 0, g_windowW, g_windowH);

  setupGL();
  initGame();

  while (g_running)
  {
    handleInput();
    tick();
    renderScene();
  }

  if (g_font)
  {
    TTF_CloseFont(g_font);
    g_font = nullptr;
  }
  shutdownGameAssets();
  TTF_Quit();
  SDL_GL_DeleteContext(g_glContext);
  SDL_DestroyWindow(g_window);
  SDL_Quit();

  return 0;
}
