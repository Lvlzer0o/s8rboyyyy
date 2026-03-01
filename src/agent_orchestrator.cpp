#include "agent_orchestrator.hpp"

void AgentOrchestrator::logEvent(const std::string& event)
{
  eventLog.push_back(event);
  if (eventLog.size() > maxLogEntries)
  {
    eventLog.pop_front();
  }
}

AgentNode* AgentOrchestrator::find(AgentRole role)
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
  initialized = true;
  logEvent("Agent mesh initialized with 5 specialists.");
}

bool AgentOrchestrator::submit(const RenderJob& job)
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

  auto* visualAgent = find(AgentRole::VisualUnderstanding);
  auto* renderExecAgent = job.dispatchToRuntime ? find(AgentRole::RenderExecution) : nullptr;
  auto* bridgeAgent = find(AgentRole::BlenderBridge);

  visualAgent->lastAction = "Parsed " + job.objPath + " for topology and material semantics";
  logEvent("Visual Understanding: analyzed " + job.objPath + ".");

  if (job.requiresPhysicsBake)
  {
    auto* physicsAgent = find(AgentRole::Physics);
    physicsAgent->lastAction = "Generated collider hulls and validated COM for " + job.objPath;
    logEvent("Physics: authored collider bake profile for " + job.objPath + ".");
  }

  if (job.requiresRigging)
  {
    auto* rigAgent = find(AgentRole::Rigging);
    rigAgent->lastAction = "Mapped deformation controls and exported rig bindings";
    logEvent("Rigging: prepared deformation control graph.");
  }

  bridgeAgent->lastAction = "Synchronized asset package with Blender bridge endpoint";
  logEvent("Blender Bridge: synchronized runtime package for " + job.objPath + ".");

  if (renderExecAgent)
  {
    renderExecAgent->lastAction = "Prepared draw-ready runtime asset manifest";
    logEvent("Render Execution: queued runtime manifest for in-game renderer.");
  }

  return true;
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
