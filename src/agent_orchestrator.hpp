#pragma once

#include <cstddef>
#include <deque>
#include <string>
#include <unordered_map>
#include <vector>

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

struct AgentRoleHash
{
  size_t operator()(AgentRole role) const noexcept
  {
    return static_cast<size_t>(role);
  }
};

struct AgentOrchestrator
{
  std::vector<AgentNode> agents;
  std::deque<std::string> eventLog;
  size_t maxLogEntries = 14;
  bool initialized = false;
  mutable bool roleIndexValid = false;
  mutable std::unordered_map<AgentRole, AgentNode*, AgentRoleHash> roleIndex;

  void logEvent(const std::string& event);
  AgentNode* find(AgentRole role);
  bool ensureOnline(AgentRole role, const std::string& missingMessage);
  void bootstrap();
  bool submit(const RenderJob& job);
  std::string formatAgentStatus(const AgentNode& agent) const;
  void invalidateRoleIndex();

private:
  bool validateSubmission(const RenderJob& job);
  void orchestrateRoleActions(const RenderJob& job,
                             AgentNode* visualAgent,
                             AgentNode* renderExecAgent,
                             AgentNode* bridgeAgent,
                             AgentNode* physicsAgent,
                             AgentNode* rigAgent);
  void logAgentActivity(AgentNode& agent,
                        const std::string& lastAction,
                        const std::string& eventLogEntry);
  void rebuildRoleIndex() const;
};

const char* agentRoleName(AgentRole role);
