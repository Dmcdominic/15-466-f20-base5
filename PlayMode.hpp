#include "Mode.hpp"

#include "Scene.hpp"
#include "WalkMesh.hpp"

#include <glm/glm.hpp>

#include <vector>
#include <deque>

struct PlayMode : Mode {
	PlayMode();
	virtual ~PlayMode();

	//functions called by main loop:
	virtual bool handle_event(SDL_Event const &, glm::uvec2 const &window_size) override;
	virtual void update(float elapsed) override;
	virtual void draw(glm::uvec2 const &drawable_size) override;

	//----- settings -----
	const float PEG_RAD = 1.5f;
	const float PICKUP_RAD = 1.2f;

	const glm::vec3 TILE_PICKUP_POS = glm::vec3(0.0f, 0.0f, 0.9f);
	const glm::quat TILE_PICKUP_ROTATION = glm::vec3(1.0f, 0.0f, M_PI * 0.5f);
	glm::quat TILE_STD_ROTATION = glm::vec3(0.0f, 0.0f, 90.0f);

	const float GATE_SPEED = 3.0f;
	float GATE_MIN_Z;
	float GATE_RAISE_HEIGHT = 4.0f;

	const float GATE_RAD = 0.5f;

	//----- game state -----

	bool attached_to_walkmesh = true;

	//input tracking:
	struct Button {
		uint8_t downs = 0;
		uint8_t pressed = 0;
	} left, right, down, up;

	//local copy of the game scene (so code can change it during gameplay):
	Scene scene;

	//player info:
	struct Player {
		WalkPoint at;
		//transform is at player's feet and will be yawed by mouse left/right motion:
		Scene::Transform *transform = nullptr;
		//camera is at player's head and will be pitched by mouse up/down motion:
		Scene::Camera *camera = nullptr;
	} player;

	enum class COLOR {
		NO_COLOR, ORANGE, PURPLE
	};

	// --- transforms ---

	//player:
	Scene::Transform* penguin = nullptr;
	Scene::Transform* pickupPt = nullptr;

	//tiles:
	struct Peg;
	struct Tile {
		Scene::Transform* transform = nullptr;
		Scene::Transform* cpyTransform = nullptr;
		Peg* peg = nullptr;
		COLOR color = COLOR::NO_COLOR;
	};
	std::vector<Tile> tiles;
	Tile* carried_tile = nullptr;

	//pegs:
	struct Peg {
		Scene::Transform* transform = nullptr;
		Tile* tile = nullptr;
		COLOR color = COLOR::NO_COLOR;
	};
	std::vector<Peg> pegs;
	glm::vec3 spinning_tile_rot = glm::vec3(0.0f, 0.0f, M_PI * 0.5f);

	//gates:
	struct Gate {
		Scene::Transform* transform = nullptr;
		bool open = false;
	};
	std::vector<Gate> gates;
};
