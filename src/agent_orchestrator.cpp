#include "agent_orchestrator.hpp"

void AgentOrchestrator::logEvent(const std::string& event)
{
  eventLog.push_back(event);
  if (eventLog.size() > maxLogEntries)
  {
    eventLog.pop_front();
  }
}

void AgentOrchestrator::invalidateRoleIndex()
{
  roleIndexValid = false;
}

void AgentOrchestrator::rebuildRoleIndex() const
{
  roleIndex.clear();
  for (auto& agent : agents)
  {
    roleIndex[agent.role] = &agent;
  }
  roleIndexValid = true;
}

AgentNode* AgentOrchestrator::find(AgentRole role)
{
  if (!roleIndexValid)
  {
    rebuildRoleIndex();
  }

  const auto it = roleIndex.find(role);
  if (it != roleIndex.end())
  {
    return it->second;
  }
  return nullptr;
}

bool AgentOrchestrator::ensureOnline(AgentRole role, const std::string& missingMessage)
{
  AgentNode* agent = find(role);
  if (!agent || !agent->online)
  {
    logEvent(missingMessage);
    return false;
  }
  return true;
}

void AgentOrchestrator::bootstrap()
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

  rebuildRoleIndex();
  initialized = true;
  logEvent("Agent mesh initialized with 5 specialists.");
}

bool AgentOrchestrator::validateSubmission(const RenderJob& job)
{
  if (!ensureOnline(AgentRole::VisualUnderstanding, "Visual Understanding agent unavailable."))
  {
    return false;
  }
  if (job.dispatchToRuntime && !ensureOnline(AgentRole::RenderExecution, "Render Execution agent unavailable."))
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
  return true;
}

void AgentOrchestrator::logAgentActivity(
  AgentNode& agent,
  const std::string& lastAction,
  const std::string& eventLogEntry)
{
  agent.lastAction = lastAction;
  logEvent(eventLogEntry);
}

void AgentOrchestrator::orchestrateRoleActions(const RenderJob& job,
                                              AgentNode* visualAgent,
                                              AgentNode* renderExecAgent,
                                              AgentNode* bridgeAgent,
                                              AgentNode* physicsAgent,
                                              AgentNode* rigAgent)
{
  logAgentActivity(
    *visualAgent,
    "Parsed " + job.objPath + " for topology and material semantics",
    "Visual Understanding: analyzed " + job.objPath + ".");

  if (physicsAgent)
  {
    logAgentActivity(
      *physicsAgent,
      "Generated collider hulls and validated COM for " + job.objPath,
      "Physics: authored collider bake profile for " + job.objPath + ".");
  }

  if (rigAgent)
  {
    logAgentActivity(*rigAgent,
      "Mapped deformation controls and exported rig bindings",
      "Rigging: prepared deformation control graph.");
  }

  logAgentActivity(*bridgeAgent,
    "Synchronized asset package with Blender bridge endpoint",
    "Blender Bridge: synchronized runtime package for " + job.objPath + ".");

  if (renderExecAgent)
  {
    logAgentActivity(*renderExecAgent,
      "Prepared draw-ready runtime asset manifest",
      "Render Execution: queued runtime manifest for in-game renderer.");
  }
}

bool AgentOrchestrator::submit(const RenderJob& job)
{
  if (!initialized)
  {
    bootstrap();
  }

  if (!validateSubmission(job))
  {
    return false;
  }

  auto* visualAgent = find(AgentRole::VisualUnderstanding);
  auto* renderExecAgent = job.dispatchToRuntime ? find(AgentRole::RenderExecution) : nullptr;
  auto* bridgeAgent = find(AgentRole::BlenderBridge);
  auto* physicsAgent = job.requiresPhysicsBake ? find(AgentRole::Physics) : nullptr;
  auto* rigAgent = job.requiresRigging ? find(AgentRole::Rigging) : nullptr;

  orchestrateRoleActions(job, visualAgent, renderExecAgent, bridgeAgent, physicsAgent, rigAgent);
  return true;
}

std::string AgentOrchestrator::formatAgentStatus(const AgentNode& agent) const
{
  return std::string(agentRoleName(agent.role)) +
    (agent.online ? " [online]" : " [offline]") +
    "  " + std::to_string(static_cast<int>(agent.latencyMs)) + "ms";
}

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
