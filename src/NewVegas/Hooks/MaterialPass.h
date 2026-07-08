#pragma once

namespace MaterialPass {
	void InitHooks();
	void BeginFrame(bool abActive);
	void CaptureScene(SceneGraph* apSceneGraph);
	void CaptureGeometry(NiGeometry* apGeometry);
	void RenderWorld();
}
