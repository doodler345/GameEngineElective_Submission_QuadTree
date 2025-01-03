#pragma once

#include "Engine/Engine.h"
#include "Engine/Rendering/RenderSystem.h"
#include "Engine/EntitySystem/EntitySystem.h"
#include "Engine/EntitySystem/Components/TextRenderComponent.h"
#include "Engine/Input/InputManager.h"

#include <SFML/Graphics.hpp>
#include <functional>

struct CollisionEntry;
class Entity;
class QuadTreeNode;

using TCollisionCallbackSignature = std::function<void(const CollisionEntry&, const CollisionEntry&, sf::Vector2f)>;

enum class EShapeType
{
	Circle,
	Box
};

struct CollisionShape
{
	EShapeType type = EShapeType::Circle;
	float radius = 0.f;
	float width = 0.f;
	float height = 0.f;
};

struct CollisionEntry
{
	int id = 0;
	bool registeredForQTEntry = false;
	bool isDeleted = false;
	bool isStatic = false;

	sf::Vector2f position;
	CollisionShape shape;

	bool isTriggerVolume = true;
	TCollisionCallbackSignature callback;

	QuadTreeNode* QTNode = nullptr;
	int QTNodeQuater = 0;

	Entity* pEntity = nullptr;

};

class QuadTreeNode
{
public:
	QuadTreeNode(sf::Vector2f centerOffset, sf::Vector2u size, int id, int parentQuarter, int maxQuarterEntries) 
: m_centerOffset(centerOffset), m_size(size), m_id(id), m_parentQuarter(parentQuarter), m_maxQuarterEntries(maxQuarterEntries)
	{
		Init();
	};
	void Init();

	~QuadTreeNode()
	{
		Engine::GetInstance()->GetRenderSystem().RemoveLine(m_renderIndexHorizontal);
		Engine::GetInstance()->GetRenderSystem().RemoveLine(m_renderIndexVertical);

		for (int i = 0; i < 4; i++)
		{
			m_TextEntities[i]->Destroy();
		}
	};

	sf::Vector2u GetSize() { return m_size; }
   	sf::Vector2f GetCenterOffset() { return m_centerOffset; }
	bool IsDestroyed() { return m_isDestroyed; }
	void Destroy() { m_isDestroyed = true; m_parent = nullptr;  }

	struct QuadTreeNode* m_parent = nullptr;
	int m_parentID = 0;
	QuadTreeNode* m_nw = nullptr;
	QuadTreeNode* m_ne = nullptr;
	QuadTreeNode* m_se = nullptr;
	QuadTreeNode* m_sw = nullptr;

	Entity* m_TextEntities[4];
	TextRenderComponent* m_TextCountEntitiesComponents[4];
	std::vector<int> m_quaterEntryIDs[4];
	int quaterEntryCount[4] = {0,0,0,0};
	int m_parentQuarter = -1;
	int m_id;

private:
	bool m_isDestroyed = false;
	int m_renderIndexHorizontal = 0;
	int m_renderIndexVertical = 0;
	const int m_maxQuarterEntries;
	sf::Vector2f m_centerOffset;
	sf::Vector2u m_size;
};

class QuadTree
{
public:
	void Init(CollisionManager* manager);
	~QuadTree()
	{
		Engine::GetInstance()->GetInputManager().Unregister(m_inputCallbackId);
		delete m_quadTreeRoot;
	}

	QuadTreeNode* AddQTEntry(CollisionEntry* entry);
	void RemoveQTEntry(CollisionEntry* entry, QuadTreeNode* m_quadtree, int quarter, bool isSubdivision);
	QuadTreeNode* SubdivideQTQuarter(QuadTreeNode* currentQuadtree, int quarter);
	QuadTreeNode* UpdateQTEntryAttributes(CollisionEntry* entry);

	QuadTreeNode* GetRootNode() { return m_quadTreeRoot;  }
	QuadTreeNode* FindQTByID(QuadTreeNode* root, int id);
	const int& MaxEntriesPerQuarter() { return m_maxQuarterEntries; }

	void OnInputPressed();


private:

	int m_inputCallbackId = 0;
	Entity* m_QTActiveTextEntity = nullptr;
	TextRenderComponent* m_QTVisibilityTextComponent = nullptr;
	bool m_showQT = false;

	QuadTreeNode* m_quadTreeRoot = nullptr;
	CollisionManager* m_collisionManager = nullptr;
	int m_quadTreeIDCounter = 0;
	int m_currentQTCount = 0;
	const int m_maxQuarterEntries = 2;
};

class CollisionManager
{
public:

	void Init();

	~CollisionManager()
	{
		Engine::GetInstance()->GetInputManager().Unregister(m_inputCallbackId);
	}

	int RegisterShape(Entity* pOwner, const CollisionShape& shape, sf::Vector2f position, bool isStatic, bool isTriggerVolume, const TCollisionCallbackSignature& callback);
	bool UnregisterShape(int id);

	CollisionEntry* FindCollisionEntryById(int id, size_t& outIndex, bool& outIsQTEntry);
	
	void UpdateShapePosition(int id, sf::Vector2f newPosition);
	void Update(float deltaSeconds);

	void OnInputPressed();


protected:

	void CheckForQTCollisions(QuadTreeNode* root);
	void CheckForQTQuarterCollisions(QuadTreeNode* root, int quarter);
	void CheckForNonQTCollisions(bool includeQTEntries = false);
	void HandleCollision_Circle_Circle(CollisionEntry& lhs, CollisionEntry& rhs);
	void HandleCollision_Circle_Box(CollisionEntry& lhs, CollisionEntry& rhs);
	void HandleCollision_Box_Box(CollisionEntry& lhs, CollisionEntry& rhs);

	sf::Vector2f GetCircleBoxSolveDirection(sf::Vector2f difference) const;


	std::vector<CollisionEntry> m_shapes_nonQT;
	std::vector<CollisionEntry> m_shapes_QT;
	std::vector<int> m_deletedShapeIndices;
	bool m_isIteratingShapes = false;
	int m_idCounter = 0;

	int m_inputCallbackId = 0;
	Entity* m_QTActiveTextEntity = nullptr;
	TextRenderComponent* m_QTActiveTextComponent = nullptr;
	Entity* m_BulletCountTextEntity = nullptr;
	TextRenderComponent* m_BulletCountTextComponent = nullptr;
	int m_bulletCount = 0;

	std::unique_ptr<QuadTree> m_quadtree;
	bool m_useQTCalculation = true;

};