//#define PRINT_QUADTREE_BEHAVIOUR
//#define PRINT_QUADTREE_COLLISIONCHECK


#include "CollisionManager.h"

#include "Engine/SFMLMath/SFMLMath.hpp"
#include "Engine/EntitySystem/EntitySystemDefinitions.h"

#include <array>
#include <cassert>

void QuadTreeNode::Init()
{
	int halfWidth = 0.5f * m_size.x;
	int halfHeigt = 0.5f * m_size.y;
	m_renderIndexHorizontal = Engine::GetInstance()->GetRenderSystem().AddLine(sf::Vector2f(m_centerOffset.x - halfWidth, m_centerOffset.y), sf::Vector2f(m_centerOffset.x + halfWidth, m_centerOffset.y));
	m_renderIndexVertical = Engine::GetInstance()->GetRenderSystem().AddLine(sf::Vector2f(m_centerOffset.x, m_centerOffset.y - halfHeigt), sf::Vector2f(m_centerOffset.x, m_centerOffset.y + halfHeigt));

	for (int quarter = 0; quarter < 4; quarter++)
	{
		m_quaterEntryIDs[quarter].assign(m_maxQuarterEntries + 1, -1);
	}

	for (int quarter = 0; quarter < 4; quarter++)
	{
		Entity* pInputPrompt = Engine::GetInstance()->GetEntitySystem().SpawnEntity<Entity>();
		m_TextEntities[quarter] = pInputPrompt;
		m_TextCountEntitiesComponents[quarter] = pInputPrompt->AddComponent<TextRenderComponent>();
		m_TextCountEntitiesComponents[quarter]->SetFontByPath("../Assets/Fonts/Roboto-Light.ttf", true);
		m_TextCountEntitiesComponents[quarter]->GetText().setString("0");
		m_TextCountEntitiesComponents[quarter]->CenterText();
		m_TextCountEntitiesComponents[quarter]->ClearText();

		float x = 1;
		float y = 1;
		switch (quarter)
		{
		case 0:
			x = -1;
			y = -1;
			break;
		case 1:
			y = -1;
			break;
		case 2:
			break;
		case 3:
			x = -1;
			break;
		}

		x *= 25.f;
		y *= 25.f;

		pInputPrompt->SetPosition(pInputPrompt->GetPosition() + sf::Vector2f(x, y) + m_centerOffset);
	}
}

void QuadTree::Init(CollisionManager* manager)
{
	m_collisionManager = manager;

	sf::Vector2u size = Engine::GetInstance()->GetRenderWindow().getSize();

	//Create Root QT
	m_quadTreeRoot = new QuadTreeNode(sf::Vector2f(0, 0), size, m_quadTreeIDCounter++, -1, m_maxQuarterEntries);
	m_currentQTCount++;


	//Input
	m_inputCallbackId = Engine::GetInstance()->GetInputManager().Register(sf::Keyboard::V, EInputEvent::Released, std::bind(&QuadTree::OnInputPressed, this));
	m_QTActiveTextEntity = Engine::GetInstance()->GetEntitySystem().SpawnEntity<Entity>();
	m_QTVisibilityTextComponent = m_QTActiveTextEntity->AddComponent<TextRenderComponent>();
	m_QTVisibilityTextComponent->SetFontByPath("../Assets/Fonts/Roboto-Light.ttf");
	m_QTVisibilityTextComponent->GetText().setString("Press 'v': Quadtree-Visualization: " + std::to_string(m_showQT));
	m_QTActiveTextEntity->SetPosition(sf::Vector2f(-0.5f * size.x + 25, 0.5f * size.y - 50));
}

void CollisionManager::Init()
{
	m_quadtree = std::make_unique<QuadTree>();
	m_quadtree->Init(this);

	sf::Vector2u size = Engine::GetInstance()->GetRenderWindow().getSize();

	//Input
	m_inputCallbackId = Engine::GetInstance()->GetInputManager().Register(sf::Keyboard::Q, EInputEvent::Released, std::bind(&CollisionManager::OnInputPressed, this));
	m_QTActiveTextEntity = Engine::GetInstance()->GetEntitySystem().SpawnEntity<Entity>();
	m_QTActiveTextComponent = m_QTActiveTextEntity->AddComponent<TextRenderComponent>();
	m_QTActiveTextComponent->SetFontByPath("../Assets/Fonts/Roboto-Light.ttf");
	m_QTActiveTextComponent->GetText().setString("Press 'q': Activate Quadtree: " + std::to_string(m_useQTCalculation));
	m_QTActiveTextEntity->SetPosition(sf::Vector2f(-0.5f * size.x + 25, 0.5f * size.y - 80));

	m_BulletCountTextEntity = Engine::GetInstance()->GetEntitySystem().SpawnEntity<Entity>();
	m_BulletCountTextComponent = m_BulletCountTextEntity->AddComponent<TextRenderComponent>();
	m_BulletCountTextComponent->SetFontByPath("../Assets/Fonts/Roboto-Light.ttf");
	m_BulletCountTextComponent->GetText().setString("Bullets: 0");
	m_BulletCountTextEntity->SetPosition(sf::Vector2f(-0.5f * size.x + 25, -0.5f * size.y + 50));
}

int CollisionManager::RegisterShape(Entity* pOwner, const CollisionShape& shape, sf::Vector2f position, bool isStatic, bool isTriggerVolume, const TCollisionCallbackSignature& callback)
{
	m_idCounter++;

	//checks if its a bullet
	CollisionEntry& entry = (shape.type == EShapeType::Circle && shape.radius == 10) ?
		m_shapes_QT.emplace_back() :
		m_shapes_nonQT.emplace_back();

	if (shape.type == EShapeType::Circle && shape.radius == 10)
	{
#ifdef PRINT_QUADTREE_BEHAVIOUR
		std::cout << "*** RegisterEntry: " << entry.id << std::endl;
#endif 
		m_bulletCount++;
		m_BulletCountTextComponent->GetText().setString("Bullets: " + std::to_string(m_bulletCount));

		if (m_useQTCalculation)
		{
			entry.registeredForQTEntry = true;
		}
	}

	entry.id = m_idCounter;
	entry.pEntity = pOwner;
	entry.shape = shape;
	entry.position = position;
	entry.callback = callback;
	entry.isStatic = isStatic;
	entry.isTriggerVolume = isTriggerVolume;

	return entry.id;
}

bool CollisionManager::UnregisterShape(int id)
{
	size_t outEntryIndex = 0;
	bool isQTEntry;
	if (CollisionEntry* pEntry = FindCollisionEntryById(id, outEntryIndex, isQTEntry))
	{
		if (isQTEntry)
		{
			if (m_isIteratingShapes)
			{
				pEntry->isDeleted = true;
				m_deletedShapeIndices.push_back(id);
				//std::cout << "QT Entry not really removed" << '\n';
			}
			else
			{
				if (m_useQTCalculation) 
					m_quadtree->RemoveQTEntry(pEntry, pEntry->QTNode, pEntry->QTNodeQuater, false);

				m_shapes_QT.erase(m_shapes_QT.begin() + outEntryIndex);

				m_bulletCount--;
				m_BulletCountTextComponent->GetText().setString("Bullets: " + std::to_string(m_bulletCount));
			}
		}
		else
		{
			if (m_isIteratingShapes)
			{
				pEntry->isDeleted = true;
				m_deletedShapeIndices.push_back(id);
				//std::cout << "Non QT Entry not really removed" << '\n';
			}
			else
			{
				m_shapes_nonQT.erase(m_shapes_nonQT.begin() + outEntryIndex);
			}
		}


		return true;
	}

	return false;
}


QuadTreeNode* QuadTree::AddQTEntry(CollisionEntry* entry)
{
	QuadTreeNode* currentQTNode = m_quadTreeRoot;
	const sf::Vector2f position = entry->position;

	//categorize to right QuadTree
	int quarter = -1;
	bool iterate = true;
	while (iterate)
	{
		sf::Vector2f centerOffset = currentQTNode->GetCenterOffset();

		if (position.x <= centerOffset.x && position.y <= centerOffset.y)
		{
			quarter = 0;

			if (currentQTNode->m_nw == nullptr || currentQTNode->m_nw->IsDestroyed()) iterate = false;
			else currentQTNode = currentQTNode->m_nw;
		}
		else if (position.x > centerOffset.x && position.y <= centerOffset.y)
		{
			quarter = 1;

			if (currentQTNode->m_ne == nullptr || currentQTNode->m_ne->IsDestroyed()) iterate = false;
			else currentQTNode = currentQTNode->m_ne;
		}
		else if (position.x > centerOffset.x && position.y > centerOffset.y)
		{
			quarter = 2;

			if (currentQTNode->m_se == nullptr || currentQTNode->m_se->IsDestroyed()) iterate = false;
			else currentQTNode = currentQTNode->m_se;
		}
		else if (position.x <= centerOffset.x && position.y > centerOffset.y)
		{
			quarter = 3;

			if (currentQTNode->m_sw == nullptr || currentQTNode->m_sw->IsDestroyed()) iterate = false;
			else currentQTNode = currentQTNode->m_sw;
		}
	}

#ifdef PRINT_QUADTREE_BEHAVIOUR
	std::cout << "+++ AddEntry " << entry->id << ": QT: " << currentQTNode->m_id << "/" << quarter << std::endl;
#endif 
	
	for (int i = 0; i < m_maxQuarterEntries + 1; i++)
	{
		if (currentQTNode->m_quaterEntryIDs[quarter][i] == -1)
		{
			currentQTNode->m_quaterEntryIDs[quarter][i] = entry->id;
			break;
		}
	}
	currentQTNode->quaterEntryCount[quarter]++;
	currentQTNode->m_TextCountEntitiesComponents[quarter]->GetText().setString(std::to_string(currentQTNode->quaterEntryCount[quarter]));


	entry->QTNodeQuater = quarter;


	if (currentQTNode->quaterEntryCount[quarter] > m_maxQuarterEntries)
	{
		SubdivideQTQuarter(currentQTNode, quarter);
	}

	return currentQTNode;
}


QuadTreeNode* QuadTree::SubdivideQTQuarter(QuadTreeNode* dividedQTNode, int quarter)
{
	sf::Vector2u size = dividedQTNode->GetSize();
	sf::Vector2u halfSize = sf::Vector2u(0.5f * size.x, 0.5 * size.y);

	sf::Vector2f offset = dividedQTNode->GetCenterOffset();
	sf::Vector2f additionalOffset;


	if (halfSize.x < 20) return nullptr; //cancle subdivide if size smaller than 2x bullet radius


	
	switch (quarter)
	{
	case 0:
		additionalOffset = sf::Vector2f(-0.25f * size.x, -0.25f * size.y);
		break;
	case 1:
		additionalOffset = sf::Vector2f(0.25f * size.x, -0.25f * size.y);
		break;
	case 2:
		additionalOffset = sf::Vector2f(0.25f * size.x, 0.25f * size.y);
		break;
	case 3:
		additionalOffset = sf::Vector2f(-0.25f * size.x, 0.25f * size.y);
		break;
	}

	QuadTreeNode* newQTNode = new QuadTreeNode(offset + additionalOffset, halfSize, m_quadTreeIDCounter++, quarter, m_maxQuarterEntries);
	m_currentQTCount++;

#ifdef PRINT_QUADTREE_BEHAVIOUR
	std::cout << std::endl << "//S// Subdivide: QT: " << dividedQTNode->m_id << "/" << quarter << " --> new QT: " << newQTNode->m_id << " @: " << &newQTNode->m_id << std::endl << std::endl;
#endif 

	newQTNode->m_parent = dividedQTNode;
	newQTNode->m_parentID = dividedQTNode->m_id;

	switch (quarter)
	{
	case 0:
		dividedQTNode->m_nw = newQTNode;
		break;
	case 1:
		dividedQTNode->m_ne = newQTNode;
		break;
	case 2:
		dividedQTNode->m_se = newQTNode;
		break;
	case 3:
		dividedQTNode->m_sw = newQTNode;
		break;
	}

	return newQTNode;
}


void QuadTree::RemoveQTEntry(CollisionEntry* entry, QuadTreeNode* QTNode, int quarter, bool ignoreMerging)
{
	bool successfullyRemoved = false;
	for (int i = 0; i < m_maxQuarterEntries + 1; i++)
	{
		if (QTNode->m_quaterEntryIDs[quarter][i] == -1) continue;

		//std::cout << "          searching Entry: Found entryIDs: " << quadtree->m_quaterEntryIDs[quarter][i] << " @: " << &quadtree->m_quaterEntryIDs[quarter][i] << '\n';
		if (QTNode->m_quaterEntryIDs[quarter][i] == entry->id)
		{
			QTNode->m_quaterEntryIDs[quarter][i] = -1;
			QTNode->quaterEntryCount[quarter]--;
			if (QTNode->quaterEntryCount[quarter] == 0)
			{
				QTNode->m_TextCountEntitiesComponents[quarter]->ClearText();
			}
			else
			{
   				QTNode->m_TextCountEntitiesComponents[quarter]->GetText().setString(std::to_string(QTNode->quaterEntryCount[quarter]));
			}
			successfullyRemoved = true;
			break;
		}
	}
	int remainingQTEntries = 0;
	for (int i = 0; i < 4; i++)
	{
		remainingQTEntries += QTNode->quaterEntryCount[i];
	}

#ifdef PRINT_QUADTREE_BEHAVIOUR
	std::cout << "--- RemoveEntry " << entry->id << ": QT: " << m_quadtree->m_id << "/" << quarter << " Remaining QT-Entities: " << remainingQTEntries << "           --> successfull: " << successfullyRemoved << '\n';
#endif 

	if (QTNode->m_id == m_quadTreeRoot->m_id) return;
	if (ignoreMerging) return;
	
	//re-merging
	
	if (remainingQTEntries <= m_maxQuarterEntries)
	{
#ifdef PRINT_QUADTREE_BEHAVIOUR
		std::cout << '\n' << ">>M<< Start Merging" << '\n';
#endif 
		if (QTNode->m_nw != nullptr || QTNode->m_ne != nullptr || QTNode->m_se != nullptr || QTNode->m_sw != nullptr)
		{
#ifdef PRINT_QUADTREE_BEHAVIOUR
			std::cout << "THIS QT HAS CHILDREN! DONT MERGE!" << '\n';
#endif 
			return;
		}

		int quadtreeId = QTNode->m_id;
		QuadTreeNode* parent = FindQTByID(m_quadTreeRoot, QTNode->m_parentID);
		assert(parent != nullptr);
		assert(parent->m_nw != nullptr || parent->m_ne != nullptr || parent->m_se != nullptr || parent->m_sw != nullptr);

		bool parentAdressNulled = false;
		if (parent->m_nw != nullptr && parent->m_nw->m_id == quadtreeId)
		{
			parentAdressNulled = true;
			parent->m_nw = nullptr;
		}
 		if (parent->m_ne != nullptr && parent->m_ne->m_id == quadtreeId) 
		{
			parentAdressNulled = true;
			parent->m_ne = nullptr;
		}
		if (parent->m_se != nullptr && parent->m_se->m_id == quadtreeId)
		{
			parentAdressNulled = true;
			parent->m_se = nullptr;
		}
		if (parent->m_sw != nullptr && parent->m_sw->m_id == quadtreeId) 
		{
			parentAdressNulled = true;
			parent->m_sw = nullptr;
		}

		assert(parentAdressNulled);

#ifdef PRINT_QUADTREE_BEHAVIOUR
		if (parentAdressNulled) 
			std::cout << "corresponding parent address nulled" << '\n';
		std::cout << "Merged QT " << m_quadtree->m_id << ": remaining entries : " << remainingQTEntries << " @: " << &m_quadtree->m_id << '\n';
#endif 
		
		QTNode->Destroy();
		m_currentQTCount--;


		for (int i = 0; i < 4; i++)
		{
			for (int j = 0; j < m_maxQuarterEntries + 1; j++)
			{
				int id = QTNode->m_quaterEntryIDs[i][j];
				if (id == -1) continue;

#ifdef PRINT_QUADTREE_BEHAVIOUR
				std::cout << "Deleting Entry " << m_quadtree->m_quaterEntryIDs[i][j] << " from merched QT, @: " << &m_quadtree->m_quaterEntryIDs[i][j] << '\n';
#endif 
				QTNode->m_quaterEntryIDs[i][j] = -1;
				QTNode->quaterEntryCount[quarter]--;
				QTNode->m_TextCountEntitiesComponents[quarter]->GetText().setString(std::to_string(QTNode->quaterEntryCount[quarter]));

				size_t outEntryIndex = 0;
				bool isQTEntry;
				CollisionEntry* entryToRebase = m_collisionManager->FindCollisionEntryById(id, outEntryIndex, isQTEntry);
				UpdateQTEntryAttributes(entryToRebase);

				AddQTEntry(entryToRebase);
			}
		}

		delete QTNode;
		QTNode = nullptr;
	}
	
}


QuadTreeNode* QuadTree::UpdateQTEntryAttributes(CollisionEntry* entry)
{
	QuadTreeNode* currentQuadtree = m_quadTreeRoot;
	const sf::Vector2f position = entry->position;

	//categorize to right QuadTree
	int quarter = -1;
	bool iterate = true;
	while (iterate)
	{
		//std::cout << "TMP: Entry " << entry->id << " in " << currentQuadtree->m_id << std::endl;
		//std::cout << "TMP: Entry " << entry->id << " in " << currentQuadtree->m_id << std::endl;

		sf::Vector2f centerOffset = currentQuadtree->GetCenterOffset();

		if (position.x <= centerOffset.x && position.y <= centerOffset.y)
		{
			quarter = 0;
			if (currentQuadtree->m_nw == nullptr || currentQuadtree->m_nw->IsDestroyed()) iterate = false;
			else currentQuadtree = currentQuadtree->m_nw;
		}
		else if (position.x > centerOffset.x && position.y <= centerOffset.y)
		{
			quarter = 1;
			if (currentQuadtree->m_ne == nullptr || currentQuadtree->m_ne->IsDestroyed()) iterate = false;
			else currentQuadtree = currentQuadtree->m_ne;
		}
		else if (position.x > centerOffset.x && position.y > centerOffset.y)
		{
			quarter = 2;
			if (currentQuadtree->m_se == nullptr || currentQuadtree->m_se->IsDestroyed()) iterate = false;
			else currentQuadtree = currentQuadtree->m_se;
		}
		else if (position.x <= centerOffset.x && position.y > centerOffset.y)
		{
			quarter = 3;
			if (currentQuadtree->m_sw == nullptr || currentQuadtree->m_sw->IsDestroyed()) iterate = false;
			else currentQuadtree = currentQuadtree->m_sw;
		}
	}

	entry->QTNodeQuater = quarter;
	entry->QTNode = currentQuadtree;
	return currentQuadtree;
}

QuadTreeNode* QuadTree::FindQTByID(QuadTreeNode* root, int id)
{
	if (root == nullptr || root->IsDestroyed())
	{
		return nullptr;
	}

#ifdef PRINT_QUADTREE_BEHAVIOUR
	std::cout << "-o   Searching QuadtreeID: " << id << " current: " << root->m_id << "/" << m_currentQTCount << " @: " << &root->m_id << '\n';
#endif 

	if (root->m_id == id)
	{
		return root;
	}

	else
	{
		QuadTreeNode* foundQT = nullptr;
		foundQT = FindQTByID(root->m_nw, id);
		if (foundQT != nullptr) return foundQT;

		foundQT = FindQTByID(root->m_ne, id);
		if (foundQT != nullptr) return foundQT;

		foundQT = FindQTByID(root->m_se, id);
		if (foundQT != nullptr) return foundQT;

		foundQT = FindQTByID(root->m_sw, id);
		if (foundQT != nullptr) return foundQT;

		return nullptr;
	}
}

void QuadTree::OnInputPressed()
{
	Engine::GetInstance()->GetRenderSystem().ToggleQuadtreeView();

	m_showQT = !m_showQT;
	m_QTVisibilityTextComponent->GetText().setString("Press 'v': Quadtree-Visualization: " + std::to_string(m_showQT));
}

void CollisionManager::OnInputPressed()
{
	if (m_useQTCalculation)
	{
		for (int i = 0; i < m_shapes_QT.size(); i++)
		{
			CollisionEntry* entryToRemove = &m_shapes_QT[i];
			m_quadtree->RemoveQTEntry(entryToRemove, entryToRemove->QTNode, entryToRemove->QTNodeQuater, false);
		}
	}
	else
	{
		for (int i = 0; i < m_shapes_QT.size(); i++)
		{
			m_shapes_QT[i].registeredForQTEntry = true;
		}

	}

	m_useQTCalculation = !m_useQTCalculation;
	m_QTActiveTextComponent->GetText().setString("Press 'q': Activate Quadtree: " + std::to_string(m_useQTCalculation));

}

void CollisionManager::UpdateShapePosition(int id, sf::Vector2f newPosition)
{
	size_t outEntryIndex = 0;
	bool isQTEntry;
	if (CollisionEntry* pEntry = FindCollisionEntryById(id, outEntryIndex, isQTEntry))
	{
		pEntry->position = newPosition;


		if (m_useQTCalculation)
		{
			if (pEntry->registeredForQTEntry)
			{
				m_quadtree->UpdateQTEntryAttributes(pEntry);
				m_quadtree->AddQTEntry(pEntry);
				pEntry->registeredForQTEntry = false;
				return;
			}


			if (pEntry->QTNode != nullptr)
			{
				int prevQuadTreeQuarter = pEntry->QTNodeQuater;
				QuadTreeNode* prevQuadTree = pEntry->QTNode;

				m_quadtree->UpdateQTEntryAttributes(pEntry);
				int updatedQuadTreeQuater = pEntry->QTNodeQuater;
				QuadTreeNode* updatedQuadTree = pEntry->QTNode;


				//same quadtree
				if (prevQuadTree->m_id == updatedQuadTree->m_id)
				{
					//other quater
					if (prevQuadTreeQuarter != updatedQuadTreeQuater)
					{
				#ifdef PRINT_QUADTREE_BEHAVIOUR
						std::cout << "!!! Entry " << pEntry->id << " simply changed quater !: " << prevQuadTree->m_id << "/" << prevQuadTreeQuarter << " --> " << prevQuadTree->m_id << "/" << updatedQuadTreeQuater << '\n';
				#endif 
						m_quadtree->RemoveQTEntry(pEntry, pEntry->QTNode, prevQuadTreeQuarter,true);
						m_quadtree->AddQTEntry(pEntry);
					}
				}
				//other quadtree
				else if (prevQuadTree->m_id != updatedQuadTree->m_id)
				{
					//quarter got subdivided
					if (updatedQuadTree->m_parent && prevQuadTree->m_id == updatedQuadTree->m_parent->m_id)
					{
				#ifdef PRINT_QUADTREE_BEHAVIOUR
						std::cout << "!!! Entry " << pEntry->id << "  detected a subdivide!" << " prev: " << prevQuadTree->m_id << "/" << prevQuadTreeQuarter << " new: " << updatedQuadTree->m_id << "/" << updatedQuadTreeQuater << '\n';
				#endif 
						m_quadtree->RemoveQTEntry(pEntry, prevQuadTree, prevQuadTreeQuarter, true);
						m_quadtree->AddQTEntry(pEntry);
					}
				
					//entry simply changed quadtree
					else
					{
				#ifdef PRINT_QUADTREE_BEHAVIOUR
						std::cout << "!!! Entry " << pEntry->id << " changed quadtree" << " prev: " << prevQuadTree->m_id << "/" << prevQuadTreeQuarter << " new: " << updatedQuadTree->m_id << "/" << updatedQuadTreeQuater << '\n';
				#endif 
						m_quadtree->RemoveQTEntry(pEntry, prevQuadTree, prevQuadTreeQuarter, false);
						m_quadtree->AddQTEntry(pEntry);
					}
				}
			}

		}

	}
}

void CollisionManager::Update(float deltaSeconds)
{
	// iterate over all shapes
	// for each shape, check for each OTHER shape whether they overlap
	// if so, invoke callback

	// ... if you have real collision resolving: Resolve collision

	m_isIteratingShapes = true;

#ifdef PRINT_QUADTREE_COLLISIONCHECK
	std::cout << '\n' << '\n' << '\n';
#endif

	if (m_useQTCalculation)
	{
		CheckForQTCollisions(m_quadtree->GetRootNode());
		CheckForNonQTCollisions();
	}
	else
	{
		CheckForNonQTCollisions(true);
	}

	m_isIteratingShapes = false;


	for (int i = 0; i < m_deletedShapeIndices.size(); i++)
	{
		//std::cout << "Clearing chached Entry " << m_deletedShapeIndices[i] << '\n';
		UnregisterShape(m_deletedShapeIndices[i]);
	}
	m_deletedShapeIndices.clear();

}


void CollisionManager::CheckForQTCollisions(QuadTreeNode* root)
{
	assert(root != nullptr && !root->IsDestroyed());

	if (root->m_nw != nullptr) CheckForQTCollisions(root->m_nw);
	else CheckForQTQuarterCollisions(root, 0);

	if (root == nullptr || root->IsDestroyed())
	{
		//std::cout << "QT got destroyed while iterating" << '\n';
		return;
	}

	if (root->m_ne != nullptr) CheckForQTCollisions(root->m_ne);
	else CheckForQTQuarterCollisions(root, 1);

	if (root == nullptr || root->IsDestroyed())
	{
		//std::cout << "QT got destroyed while iterating" << '\n';
		return;
	}

	if (root->m_se != nullptr) CheckForQTCollisions(root->m_se);
	else CheckForQTQuarterCollisions(root, 2);

	if (root == nullptr || root->IsDestroyed())
	{
		//std::cout << "QT got destroyed while iterating" << '\n';
		return;
	}

	if (root->m_sw != nullptr) CheckForQTCollisions(root->m_sw);
	else CheckForQTQuarterCollisions(root, 3);
}

void CollisionManager::CheckForQTQuarterCollisions(QuadTreeNode* root, int quarter)
{
	int maxEntriesPerQuater = m_quadtree->MaxEntriesPerQuarter();
	std::vector<int> foundEntries(maxEntriesPerQuater, -1);

	for (int i = 0; i < maxEntriesPerQuater; i++)
	{
		if (root->m_quaterEntryIDs[quarter][i] != -1)
		{
			foundEntries[i] = root->m_quaterEntryIDs[quarter][i];
		}
	}

#ifdef PRINT_QUADTREE_COLLISIONCHECK

	std::cout << "QT " << root->m_id << "/" << quarter << " holds entries: ";
	for (int i = 0; i < maxEntriesPerQuater; i++)
	{
		int foundEntryID = foundEntries[i];

		if (foundEntryID == -1) 
			continue;

		std::cout << foundEntries[i] << " ";
	}
	std::cout << '\n';

#endif 

	
	for (int i = 0; i < maxEntriesPerQuater - 1; i++)
	{
		if (foundEntries[i] == -1)
			continue;

		size_t x;
		bool isQTEntry;

		CollisionEntry& lhs = *FindCollisionEntryById(foundEntries[i], x, isQTEntry);

		if (lhs.isDeleted) 
			continue;


		for (int k = i + 1; k < maxEntriesPerQuater; k++)
		{
			if (foundEntries[k] == -1)
				continue;

			CollisionEntry& rhs = *FindCollisionEntryById(foundEntries[k], x, isQTEntry);

			if (rhs.isDeleted) 
				continue;

			//every bullet has a circle-shape collider, so no shape check here
			HandleCollision_Circle_Circle(lhs, rhs);
		}
	}
	

}

void CollisionManager::CheckForNonQTCollisions(bool includeQTEntries)
{

	//check other-than-bullets (Tanks, Walls...) for Collisions
	for (int i = 0; i < m_shapes_nonQT.size(); i++)
	{
		CollisionEntry& lhs = m_shapes_nonQT[i];

		if (lhs.isDeleted)
			continue;

		//check for Bullets-Collision
		for (int k = 0; k < m_shapes_QT.size(); k++)
		{
			CollisionEntry& rhs = m_shapes_QT[k];

			if (rhs.isDeleted)
				continue;

			switch (lhs.shape.type)
			{
			case EShapeType::Circle:
			{
				switch (rhs.shape.type)
				{
				case EShapeType::Circle:
					HandleCollision_Circle_Circle(lhs, rhs);
					break;
				case EShapeType::Box:
					HandleCollision_Circle_Box(lhs, rhs);
					break;
				}
				break;
			}

			case EShapeType::Box:
			{
				switch (rhs.shape.type)
				{
				case EShapeType::Circle:
					HandleCollision_Circle_Box(rhs, lhs);
					break;
				case EShapeType::Box:
					HandleCollision_Box_Box(lhs, rhs);
					break;
				}
				break;
			}
			}
		}

		//check for other-than-bullets Collisions
		for (int k = i + 1; k < m_shapes_nonQT.size(); k++)
		{
			CollisionEntry& rhs = m_shapes_nonQT[k];

			if (rhs.isDeleted || i == k)
				continue;

			switch (lhs.shape.type)
			{
			case EShapeType::Circle:
			{
				switch (rhs.shape.type)
				{
				case EShapeType::Circle:
					HandleCollision_Circle_Circle(lhs, rhs);
					break;
				case EShapeType::Box:
					HandleCollision_Circle_Box(lhs, rhs);
					break;
				}
				break;
			}
			case EShapeType::Box:
			{
				switch (rhs.shape.type)
				{
				case EShapeType::Circle:
					HandleCollision_Circle_Box(rhs, lhs);
					break;
				case EShapeType::Box:
					HandleCollision_Box_Box(lhs, rhs);
					break;
				}
				break;
			}
			}
		}
	}


	//include QT-Entries if non-QT calculation should be used for them instead QT Calculation 
	//(e.g. for testing performance)
	if (includeQTEntries)
	{
		if (m_shapes_QT.size() == 0) 
			return;

		for (int i = 0; i < m_shapes_QT.size() - 1; i++)
		{
			CollisionEntry& lhs = m_shapes_QT[i];

			if (lhs.isDeleted)
				continue;

			for (int k = i + 1; k < m_shapes_QT.size(); k++)
			{
				CollisionEntry& rhs = m_shapes_QT[k];

				if (rhs.isDeleted)
					continue;

				HandleCollision_Circle_Circle(lhs, rhs);
			}
		}
	}
}





void CollisionManager::HandleCollision_Circle_Circle(CollisionEntry& lhs, CollisionEntry& rhs)
{
	CollisionShape& lhsCircle = lhs.shape;
	CollisionShape& rhsCircle = rhs.shape;

	const sf::Vector2f lhsToRhs = lhs.position - rhs.position;
	const float distance = sf::getLength(lhsToRhs);
	const float distanceBetweenCircles = distance - (lhsCircle.radius + rhsCircle.radius);
	const sf::Vector2f normalizedLhsToRhs = sf::getNormalized(lhsToRhs);

	if (distanceBetweenCircles < 0.f)
	{
		sf::Vector2f resolveVectorLhs;
		sf::Vector2f resolveVectorRhs;

		if (lhs.isTriggerVolume || rhs.isTriggerVolume)
		{
			//Handle overlap
		}
		else if (!lhs.isTriggerVolume && !rhs.isTriggerVolume)
		{
			//Handle collision resolve
			if (lhs.isStatic != rhs.isStatic)
			{
				CollisionEntry& movableEntry = lhs.isStatic ? rhs : lhs;
				sf::Vector2f& resolveVector = lhs.isStatic ? resolveVectorRhs : resolveVectorLhs;
				resolveVector = (lhs.isStatic ? normalizedLhsToRhs : -normalizedLhsToRhs) * distanceBetweenCircles;

				movableEntry.pEntity->SetPosition(movableEntry.position + resolveVector);
			}
			else if (!lhs.isStatic && !rhs.isStatic)
			{
				//Both movable move away from each other
				resolveVectorLhs = normalizedLhsToRhs * distanceBetweenCircles * -0.5f;
				resolveVectorRhs = normalizedLhsToRhs * distanceBetweenCircles * 0.5f;

				lhs.pEntity->SetPosition(lhs.position + resolveVectorLhs);
				rhs.pEntity->SetPosition(rhs.position + resolveVectorRhs);
			}
			else
			{
				//Both are static, do nothing (both immovable)
			}
		}

		if (lhs.callback)
			lhs.callback(lhs, rhs, resolveVectorLhs);

		if (rhs.callback)
			rhs.callback(rhs, lhs, resolveVectorRhs);
	}
}

void CollisionManager::HandleCollision_Circle_Box(CollisionEntry& lhs, CollisionEntry& rhs)
{
	CollisionShape& lhsCircle = lhs.shape;
	CollisionShape& rhsBox = rhs.shape;

	// get center point circle first 
	// calculate AABB info (center, half-extents)
	sf::Vector2f aabb_half_extents(rhsBox.width * 0.5f, rhsBox.height * 0.5f);
	// get difference vector between both centers
	sf::Vector2f difference = lhs.position - rhs.position;

	sf::Vector2f clamped;
	clamped.x = std::clamp(difference.x, -aabb_half_extents.x, aabb_half_extents.x);
	clamped.y = std::clamp(difference.y, -aabb_half_extents.y, aabb_half_extents.y);

	// now that we know the clamped values, add this to AABB_center and we get the value of box closest to circle
	sf::Vector2f closest = rhs.position + clamped;

	// now retrieve vector between center circle and closest point AABB and check if length < radius
	difference = closest - lhs.position;

	if (difference.x == 0.f && difference.y == 0.f)
		return;

	const float diffLength = sf::getLength(difference);

	if (diffLength < lhsCircle.radius)
	{
		const float penetrationDistance = lhsCircle.radius - diffLength;

		sf::Vector2f resolveVectorLhs;
		sf::Vector2f resolveVectorRhs;

		if (lhs.isTriggerVolume || rhs.isTriggerVolume)
		{
			//Handle overlap
		}
		else if (!lhs.isTriggerVolume && !rhs.isTriggerVolume)
		{
			//Handle collision resolve
			if (lhs.isStatic != rhs.isStatic)
			{
				//Both movable move away from each other
				sf::Vector2f& resolveVector = lhs.isStatic ? resolveVectorRhs : resolveVectorLhs;
				CollisionEntry& movableEntry = lhs.isStatic ? rhs : lhs;
				resolveVector = GetCircleBoxSolveDirection(difference) * penetrationDistance;

				movableEntry.pEntity->SetPosition(movableEntry.position - resolveVector);
			}
			else if (!lhs.isStatic && !rhs.isStatic)
			{
				//Both movable move away from each other
				sf::Vector2f solveDirection = GetCircleBoxSolveDirection(difference);

				resolveVectorLhs = solveDirection * penetrationDistance * -0.5f;
				resolveVectorRhs = solveDirection * penetrationDistance * 0.5f;

				lhs.pEntity->SetPosition(lhs.position + resolveVectorLhs);
				rhs.pEntity->SetPosition(rhs.position + resolveVectorRhs);
			}
			else
			{
				//Both are static, do nothing (both immovable)
			}
		}

		if (lhs.callback)
			lhs.callback(lhs, rhs, resolveVectorLhs);

		if (rhs.callback)
			rhs.callback(rhs, lhs, resolveVectorRhs);
	}
}

void CollisionManager::HandleCollision_Box_Box(CollisionEntry& lhs, CollisionEntry& rhs)
{
	// to be implemented
}

sf::Vector2f CollisionManager::GetCircleBoxSolveDirection(sf::Vector2f difference) const
{
	static const std::array<sf::Vector2f, 4> dirs = {
		sf::Vector2f(0.0f, 1.0f),	// up
		sf::Vector2f(1.0f, 0.0f),	// right
		sf::Vector2f(0.0f, -1.0f),	// down
		sf::Vector2f(-1.0f, 0.0f)	// left
	};

	float max = 0.0f;
	unsigned int bestMatch = -1;
	for (unsigned int i = 0; i < 4; i++)
	{
		float dot_product = sf::dot(sf::getNormalized(difference), dirs[i]);
		if (dot_product > max)
		{
			max = dot_product;
			bestMatch = i;
		}
	}

	return dirs[bestMatch];
}

CollisionEntry* CollisionManager::FindCollisionEntryById(int id, size_t& outIndex, bool& outIsQTEntry)
{
	for (int i = 0; i < m_shapes_QT.size(); i++)
	{
		if (m_shapes_QT[i].id == id)
		{
			outIndex = i;
			outIsQTEntry = true;
			return &m_shapes_QT[i];
		}
	}

	for (int i = 0; i < m_shapes_nonQT.size(); i++)
	{
		if (m_shapes_nonQT[i].id == id)
		{
			outIndex = i;
			outIsQTEntry = false;
			return &m_shapes_nonQT[i];
		}
	}

	return nullptr;
}
