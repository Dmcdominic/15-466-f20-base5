#include "PlayMode.hpp"

#include "LitColorTextureProgram.hpp"

#include "DrawLines.hpp"
#include "Mesh.hpp"
#include "Load.hpp"
#include "gl_errors.hpp"
#include "data_path.hpp"

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>

#include <random>

GLuint phonebank_meshes_for_lit_color_texture_program = 0;
Load< MeshBuffer > phonebank_meshes(LoadTagDefault, []() -> MeshBuffer const * {
	MeshBuffer const *ret = new MeshBuffer(data_path("phone-bank.pnct"));
	phonebank_meshes_for_lit_color_texture_program = ret->make_vao_for_program(lit_color_texture_program->program);
	return ret;
});

Load< Scene > phonebank_scene(LoadTagDefault, []() -> Scene const * {
	return new Scene(data_path("phone-bank.scene"), [&](Scene &scene, Scene::Transform *transform, std::string const &mesh_name){
		Mesh const &mesh = phonebank_meshes->lookup(mesh_name);

		scene.drawables.emplace_back(transform);
		Scene::Drawable &drawable = scene.drawables.back();

		drawable.pipeline = lit_color_texture_program_pipeline;

		drawable.pipeline.vao = phonebank_meshes_for_lit_color_texture_program;
		drawable.pipeline.type = mesh.type;
		drawable.pipeline.start = mesh.start;
		drawable.pipeline.count = mesh.count;

	});
});

WalkMesh const *walkmesh = nullptr;
Load< WalkMeshes > phonebank_walkmeshes(LoadTagDefault, []() -> WalkMeshes const * {
	WalkMeshes *ret = new WalkMeshes(data_path("phone-bank.w"));
	walkmesh = &ret->lookup("WalkMesh");
	return ret;
});

PlayMode::PlayMode() : scene(*phonebank_scene) {
	//create a player transform:
	scene.transforms.emplace_back();
	player.transform = &scene.transforms.back();

	//create a player camera attached to a child of the player transform:
	scene.transforms.emplace_back();
	scene.cameras.emplace_back(&scene.transforms.back());
	player.camera = &scene.cameras.back();
	player.camera->fovy = glm::radians(60.0f);
	player.camera->near = 0.01f;
	player.camera->transform->parent = player.transform;

	//player's eyes are 1.8 units above the ground:
	player.camera->transform->position = glm::vec3(0.0f, 0.0f, 1.8f);

	//rotate camera facing direction (-z) to player facing direction (+y):
	player.camera->transform->rotation = glm::angleAxis(glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));

	//start player walking at nearest walk point:
	player.at = walkmesh->nearest_walk_point(player.transform->position);


	//init tiles vector
	tiles = std::vector<Tile>(2);
	tiles[0].color = COLOR::ORANGE;
	tiles[1].color = COLOR::PURPLE;

	//init pegs vector
	pegs = std::vector<Peg>(2);
	pegs[0].color = COLOR::ORANGE;
	pegs[1].color = COLOR::PURPLE;

	//init gates vector
	gates = std::vector<Gate>(2);

	// get pointer to each transform for reference
	for (auto& transform : scene.transforms) {
		if (transform.name == "Penguin") penguin = &transform;
		if (transform.name == "PickupPt") pickupPt = &transform;
		if (transform.name == "TileOrange") tiles[0].transform = &transform;
		if (transform.name == "TileOrangeCpy") tiles[0].cpyTransform = &transform;
		if (transform.name == "TilePurple") tiles[1].transform = &transform;
		if (transform.name == "TilePurpleCpy") tiles[1].cpyTransform = &transform;
		if (transform.name == "Peg0") pegs[0].transform = &transform;
		if (transform.name == "Peg1") pegs[1].transform = &transform;
		if (transform.name == "Gate0") gates[0].transform = &transform;
		if (transform.name == "Gate1") gates[1].transform = &transform;
	}
	// Check for missing transforms
	if (penguin == nullptr) throw std::runtime_error("penguin not found.");
	if (pickupPt == nullptr) throw std::runtime_error("pickupPt not found.");
	// Check for missing transforms in the vectors
	for (auto tileIter = tiles.begin(); tileIter != tiles.end(); tileIter++) {
		if (tileIter->transform == nullptr || tileIter->cpyTransform == nullptr) {
			std::cerr << "missing tile " << (tileIter - tiles.begin()) << std::endl;
			throw std::runtime_error("tile not found.");
		}
	}
	for (auto pegIter = pegs.begin(); pegIter != pegs.end(); pegIter++) {
		if (pegIter->transform == nullptr) {
			std::cerr << "missing peg " << (pegIter - pegs.begin()) << std::endl;
			throw std::runtime_error("peg not found.");
		}
	}
	for (auto gateIter = gates.begin(); gateIter != gates.end(); gateIter++) {
		if (gateIter->transform == nullptr) {
			std::cerr << "missing gate " << (gateIter - gates.begin()) << std::endl;
			throw std::runtime_error("gate not found.");
		}
	}
	
	// set pickupPt's parent to the player
	pickupPt->parent = player.transform;

	// init some standards
	TILE_STD_ROTATION = tiles[0].transform->rotation;
	GATE_MIN_Z = gates[0].transform->position.z;
}

PlayMode::~PlayMode() {
}

bool PlayMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {

	if (evt.type == SDL_KEYDOWN) {
		if (evt.key.keysym.sym == SDLK_ESCAPE) {
			SDL_SetRelativeMouseMode(SDL_FALSE);
			return true;
		} else if (evt.key.keysym.sym == SDLK_a) {
			left.downs += 1;
			left.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_d) {
			right.downs += 1;
			right.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_w) {
			up.downs += 1;
			up.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_s) {
			down.downs += 1;
			down.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_q) {  // QUIT
			quit = true;
			return true;
    } else if (evt.key.keysym.sym == SDLK_e) { // PICK UP button
			// Drop tile, if you're already carrying one
      if (carried_tile != nullptr) {
        // Check if you're close enough to place IT'S COPY on a peg
        for (auto pegIter = pegs.begin(); pegIter != pegs.end(); pegIter++) {
          if (pegIter->tile != nullptr) {
            continue;
          }
          glm::vec3 pegPos = glm::vec3(pegIter->transform->make_local_to_world()[3]);
          glm::vec3 carriedTilePos = glm::vec3(carried_tile->cpyTransform->make_local_to_world()[3]);
          float dist = glm::length(pegPos - carriedTilePos);
          //std::cout << "dist: " << dist << std::endl;
          if (dist < PEG_RAD) {
            pegIter->tile = carried_tile;
            carried_tile->peg = &(*pegIter);
            carried_tile->transform->position = glm::vec3(pegPos.y, pegPos.x, -5.0f - pegPos.z);
            carried_tile->transform->rotation = TILE_STD_ROTATION;
            //carried_tile->transform->parent = carried_tile->peg->transform;
						carried_tile->transform->parent = nullptr;
            carried_tile = nullptr;
            return true;
          }
        }
        // Otherwise, just drop it
        carried_tile->transform->position = glm::vec3(carried_tile->transform->make_local_to_world()[3]);
        carried_tile->transform->rotation = TILE_STD_ROTATION;
        carried_tile->transform->parent = nullptr;
        carried_tile = nullptr;
        return true;
      }
      // Pickup nearby tile, if there is one
      for (auto tilesIter = tiles.begin(); tilesIter != tiles.end(); tilesIter++) {
        glm::vec3 tilePos = glm::vec3(tilesIter->transform->make_local_to_world()[3]);
        glm::vec3 pickupPtPos = glm::vec3(pickupPt->make_local_to_world()[3]);
        float dist = glm::length(tilePos - pickupPtPos);
        //std::cout << "dist: " << dist << std::endl;
        if (dist < PICKUP_RAD) {
          carried_tile = &(*tilesIter);
          if (carried_tile->peg != nullptr) {
            carried_tile->peg->tile = nullptr;
            carried_tile->peg = nullptr;
          }
          carried_tile->transform->position = TILE_PICKUP_POS;
          carried_tile->transform->rotation = TILE_PICKUP_ROTATION;
          carried_tile->transform->parent = pickupPt;
          return true;
        }
      }
    }
	} else if (evt.type == SDL_KEYUP) {
		if (evt.key.keysym.sym == SDLK_a) {
			left.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_d) {
			right.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_w) {
			up.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_s) {
			down.pressed = false;
			return true;
		}
	} else if (evt.type == SDL_MOUSEBUTTONDOWN) {
		if (SDL_GetRelativeMouseMode() == SDL_FALSE) {
			SDL_SetRelativeMouseMode(SDL_TRUE);
			return true;
		}
	} else if (evt.type == SDL_MOUSEMOTION) {
		if (SDL_GetRelativeMouseMode() == SDL_TRUE) {
			glm::vec2 motion = glm::vec2(
				evt.motion.xrel / float(window_size.y),
				-evt.motion.yrel / float(window_size.y)
			);
			glm::vec3 up = walkmesh->to_world_smooth_normal(player.at);
			player.transform->rotation = glm::angleAxis(-motion.x * player.camera->fovy, up) * player.transform->rotation;

			float pitch = glm::pitch(player.camera->transform->rotation);
			pitch += motion.y * player.camera->fovy;
			//camera looks down -z (basically at the player's feet) when pitch is at zero.
			pitch = std::min(pitch, 0.95f * 3.1415926f);
			pitch = std::max(pitch, 0.05f * 3.1415926f);
			player.camera->transform->rotation = glm::angleAxis(pitch, glm::vec3(1.0f, 0.0f, 0.0f));

			return true;
		}
	}

	return false;
}

void PlayMode::update(float elapsed) {
	//player walking:
	{
		//combine inputs into a move:
		constexpr float PlayerSpeed = 3.0f;
		glm::vec2 move = glm::vec2(0.0f);
		if (left.pressed && !right.pressed) move.x =-1.0f;
		if (!left.pressed && right.pressed) move.x = 1.0f;
		if (down.pressed && !up.pressed) move.y =-1.0f;
		if (!down.pressed && up.pressed) move.y = 1.0f;

		//make it so that moving diagonally doesn't go faster:
		if (move != glm::vec2(0.0f)) move = glm::normalize(move) * PlayerSpeed * elapsed;

		//get move in world coordinate system:
		glm::vec3 remain = player.transform->make_local_to_world() * glm::vec4(move.x, move.y, 0.0f, 0.0f);

		//using a for() instead of a while() here so that if walkpoint gets stuck in
		// some awkward case, code will not infinite loop:
		for (uint32_t iter = 0; iter < 10; ++iter) {
			if (remain == glm::vec3(0.0f)) break;
			WalkPoint end;
			float time;
			walkmesh->walk_in_triangle(player.at, remain, &end, &time);
			player.at = end;
			if (time == 1.0f) {
				//finished within triangle:
				remain = glm::vec3(0.0f);
				break;
			}
			//some step remains:
			remain *= (1.0f - time);
			//try to step over edge:
			glm::quat rotation;
			if (walkmesh->cross_edge(player.at, &end, &rotation)) {
				//stepped to a new triangle:
				player.at = end;
				//rotate step to follow surface:
				remain = rotation * remain;
			} else {
				//ran into a wall, bounce / slide along it:
				glm::vec3 const &a = walkmesh->vertices[player.at.indices.x];
				glm::vec3 const &b = walkmesh->vertices[player.at.indices.y];
				glm::vec3 const &c = walkmesh->vertices[player.at.indices.z];
				glm::vec3 along = glm::normalize(b-a);
				glm::vec3 normal = glm::normalize(glm::cross(b-a, c-a));
				glm::vec3 in = glm::cross(normal, along);

				//check how much 'remain' is pointing out of the triangle:
				float d = glm::dot(remain, in);
				if (d < 0.0f) {
					//bounce off of the wall:
					remain += (-1.25f * d) * in;
				} else {
					//if it's just pointing along the edge, bend slightly away from wall:
					remain += 0.01f * d * in;
				}
			}
		}

		if (remain != glm::vec3(0.0f)) {
			std::cout << "NOTE: code used full iteration budget for walking." << std::endl;
		}

		if (attached_to_walkmesh) {
			//update player's position to respect walking:
			player.transform->position = walkmesh->to_world_point(player.at);

			//bound the player to the (closed) gates
			for (auto gateIter = gates.begin(); gateIter != gates.end(); gateIter++) {
				if (!gateIter->open && player.transform->position.y > gateIter->transform->position.y - GATE_RAD) {
					player.transform->position.y = std::min(gateIter->transform->position.y - GATE_RAD, player.transform->position.y);
					player.at = walkmesh->nearest_walk_point(player.transform->position);
					break;
				}
			}

			//update player's rotation to respect local (smooth) up-vector:
			glm::quat adjust = glm::rotation(
				player.transform->rotation * glm::vec3(0.0f, 0.0f, 1.0f), //current up vector
				walkmesh->to_world_smooth_normal(player.at) //smoothed up vector at walk location
			);
			player.transform->rotation = glm::normalize(adjust * player.transform->rotation);
		} else {
			player.transform->position.x += elapsed * PlayerSpeed * 0.02f;
			player.transform->position.y += elapsed * PlayerSpeed;
		}

		if (player.transform->position.y > 13.0f) {
			attached_to_walkmesh = false;
		}

		// Update penguin's position according to player
		penguin->position = glm::vec3(player.transform->position.y, player.transform->position.x, -5.0f - player.transform->position.z);
		// Update each tile's cpy position
		for (auto tilesIter = tiles.begin(); tilesIter != tiles.end(); tilesIter++) {
			glm::vec3 tilePos = glm::vec3(tilesIter->transform->make_local_to_world()[3]);
			tilesIter->cpyTransform->position = glm::vec3(tilePos.y, tilePos.x, -5.0f - tilePos.z);
			tilesIter->cpyTransform->rotation = tilesIter->transform->rotation;
		}	
	}

	//set gates open/closed according to pegs, and update their position
	{
		gates[0].open = (pegs[0].tile != nullptr && pegs[0].tile->color == pegs[0].color);
		gates[1].open = (pegs[1].tile != nullptr && pegs[1].tile->color == pegs[1].color);
		// Update gates position
		for (auto gateIter = gates.begin(); gateIter != gates.end(); gateIter++) {
			if (gateIter->open) {
				gateIter->transform->position.z = std::min(GATE_MIN_Z + GATE_RAISE_HEIGHT, gateIter->transform->position.z + GATE_SPEED * elapsed);
			}
			else {
				gateIter->transform->position.z = std::max(GATE_MIN_Z, gateIter->transform->position.z - GATE_SPEED * 1.5f * elapsed);
			}
		}
	}

	// spin any tiles that are on pegs
	{
		spinning_tile_rot.x += elapsed;
		for (auto pegIter = pegs.begin(); pegIter != pegs.end(); pegIter++) {
			if (pegIter->tile != nullptr) {
				if (pegIter->color == COLOR::NO_COLOR || pegIter->color == pegIter->tile->color) {
					// color is correct
					pegIter->tile->transform->rotation = spinning_tile_rot;
				}
				else {
					// color is incorrect

				}
			}
		}
	}


	//reset button press counters:
	left.downs = 0;
	right.downs = 0;
	up.downs = 0;
	down.downs = 0;
}

void PlayMode::draw(glm::uvec2 const &drawable_size) {
	//update camera aspect ratio for drawable:
	player.camera->aspect = float(drawable_size.x) / float(drawable_size.y);

	//set up light type and position for lit_color_texture_program:
	// TODO: consider using the Light(s) in the scene to do this
	glUseProgram(lit_color_texture_program->program);
	glUniform1i(lit_color_texture_program->LIGHT_TYPE_int, 1);
	glUniform3fv(lit_color_texture_program->LIGHT_DIRECTION_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f,-1.0f)));
	glUniform3fv(lit_color_texture_program->LIGHT_ENERGY_vec3, 1, glm::value_ptr(glm::vec3(1.0f, 1.0f, 0.95f)));
	glUseProgram(0);

	glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
	glClearDepth(1.0f); //1.0 is actually the default value to clear the depth buffer to, but FYI you can change it.
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS); //this is the default depth comparison function, but FYI you can change it.

	scene.draw(*player.camera);

	{ //use DrawLines to overlay some text:
		glDisable(GL_DEPTH_TEST);
		float aspect = float(drawable_size.x) / float(drawable_size.y);
		DrawLines lines(glm::mat4(
			1.0f / aspect, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f
		));

		constexpr float H = 0.09f;
		lines.draw_text("Mouse motion looks; WASD moves; escape ungrabs mouse",
			glm::vec3(-aspect + 0.1f * H, -1.0 + 0.1f * H, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0x00, 0x00, 0x00, 0x00));
		float ofs = 2.0f / drawable_size.y;
		lines.draw_text("Mouse motion looks; WASD moves; escape ungrabs mouse",
			glm::vec3(-aspect + 0.1f * H + ofs, -1.0 + + 0.1f * H + ofs, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0xff, 0xff, 0xff, 0x00));
	}
	GL_ERRORS();
}
