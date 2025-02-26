#pragma once
//
// Created by jcazm on 7/27/2024.
//

struct ActorRenderData {
	Actor* mActor = nullptr;
	glm::vec3 position{};
	glm::vec2 rotation{};
	glm::vec2 mHeadRot{};
	bool glint = false;
	bool mIgnoreLighting = false;
	bool mIsInUI = false;
	float mDeltaTime = 0.0f;
	int mModelObjId = 0;
	float mModelSize = 0.0f;
	class AnimationComponent* mAnimationComponent = nullptr;
	class MolangVariableMap* mVariables = nullptr;
};