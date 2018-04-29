#include "Renderer.hpp"
#include "Object.hpp"
#include "input/Input.hpp"
#include "helpers/InterfaceUtilities.hpp"
#include "helpers/Logger.hpp"
#include <glm/gtx/norm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <stdio.h>
#include <vector>


Renderer::~Renderer(){}

Renderer::Renderer(Config & config) : _config(config) {
	
	// Initial render resolution.
	_renderResolution = (_config.internalVerticalResolution/_config.screenResolution[1]) * _config.screenResolution;
	
	defaultGLSetup();
	
	_quad.init("passthrough");
	// Setup camera parameters.
	_camera.projection(config.screenResolution[0]/config.screenResolution[1], 1.3f, 0.1f, 8000.0f);
	for(int i = 0; i < 8; ++i){
		//Resources::manager().getProgram("object_basic")->registerTexture("textures["+std::to_string(i)+"]", i);
		//Resources::manager().getProgram("object_basic")->registerTexture("cubemaps["+std::to_string(i)+"]", 8+i);
	}
	Resources::manager().getProgram("object_basic")->registerTexture("textures", 0);
	Resources::manager().getProgram("object_basic")->registerTexture("cubemaps", 1);
	
	
	std::vector<std::string> files = ImGui::listFiles("../../../data/mystv/", false, false, {"age"});
	
	loadAge("../../../data/uru/spyroom.age");
}



void Renderer::draw(){
	glViewport(0, 0, GLsizei(_config.screenResolution[0]), GLsizei(_config.screenResolution[1]));
	glClearColor(0.45f,0.45f, 0.5f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	
	// FIXME: move to attributes.
	static int textureId = 0;
	static bool showTextures = false;
	static int objectId = 0;
	static bool showObject = false;
	static bool wireframe = true;
	static bool doCulling = true;
	static float cullingDistance = 1500.0f;
	static int drawCount = 0;
	if (ImGui::Begin("Infos")) {
		
		ImGui::Text("%2.1f FPS (%2.1f ms)", ImGui::GetIO().Framerate, ImGui::GetIO().DeltaTime*1000.0f);
		ImGui::Text("Age: %s", (_age ? _age->getName().c_str() : "None"));
		ImGui::Text("Draws: %i/%lu", drawCount, _age->objects().size());
		if(showTextures){
			ImGui::Text("Current: %s", _age->textures()[textureId].c_str());
		}
		if(showObject){
			ImGui::Text("Current: %s", _age->objects()[objectId]->getName().c_str());
		}
	}
	ImGui::End();
	
	if (ImGui::Begin("Settings")) {
		static int current_item_id = 0;
		
		if(ImGui::Button("Load .age file...")){
			ImGui::OpenFilePicker("Load Age");
		}
		std::string selectedFile;
		if(ImGui::BeginFilePicker("Load Age", "Load a .age file.", "../../../data/", selectedFile, false, false, {"age"})){
			loadAge(selectedFile);
			current_item_id = 0;
			textureId = 0;
			showTextures = false;
		}
		
		
		auto linkingNameProvider = [](void* data, int idx, const char** out_text) {
			const std::vector<std::string>* arr = (std::vector<std::string>*)data;
			*out_text = (*arr)[idx].c_str();
			return true;
		};
		if (ImGui::Combo("Linking point", &current_item_id, linkingNameProvider, (void*)&(_age->linkingNames()), _age->linkingNames().size())) {
			if (current_item_id >= 0) {
				_camera.setCenter(_age->linkingPoints().at(_age->linkingNames()[current_item_id]));
			}
			
		}
		
		
		ImGui::Checkbox("Wireframe", &wireframe);
		ImGui::Checkbox("Culling", &doCulling);
		ImGui::SliderFloat("Culling dist.", &cullingDistance, 10.0f, 3000.0f);
		ImGui::Checkbox("Show textures", &showTextures);
		ImGui::SliderInt("Texture ID", &textureId, 0, _age->textures().size()-1);
		
		ImGui::Checkbox("Show object", &showObject);
		ImGui::SliderInt("Object ID", &objectId, 0, _age->objects().size()-1);
		
	}
	ImGui::End();
	Log::Info().display();
	
	if(showTextures){
		_quad.draw(Resources::manager().getTexture(_age->textures()[textureId]).id);
		return;
	}
	
	glEnable(GL_DEPTH_TEST);
	checkGLError();
	if(showObject){
		const auto objectToShow = _age->objects()[objectId];
		
		if(wireframe){
			objectToShow->drawDebug(_camera.view() , _camera.projection());
		} else {
			objectToShow->draw(_camera.view() , _camera.projection());
		}
		
	} else {
		const glm::mat4 viewproj = _camera.projection() * _camera.view();
		drawCount = 0;
		auto objects = _age->objectsClone();
		/*auto& camera = _camera;
		std::sort(objects.begin(), objects.end(), [&camera](const std::shared_ptr<Object> & leftObj, const std::shared_ptr<Object> & rightObj){
			
			// Compute both distances to camera.
			const float leftDist = glm::length2(leftObj->getCenter() - camera.getPosition());
			const float rightDist = glm::length2(rightObj->getCenter() - camera.getPosition());
			// We want to first render objects further away from the camera.
			return leftDist > rightDist;
			
		});*/
		
		for(const auto & object : objects){
			if(doCulling &&
			   (glm::length2(object->getCenter() - _camera.getPosition()) > cullingDistance*cullingDistance ||
				// !object->isVisible(_camera.getPosition(), _camera.getDirection()) || (
				 !object->isVisible(_camera.getPosition(), viewproj)
				)){
				continue;
			}
			++drawCount;
			if(wireframe){
				object->drawDebug(_camera.view() , _camera.projection());
			} else {
				object->draw(_camera.view() , _camera.projection());
			}
		}
	}
	
	const float scale = glm::length(_camera.getDirection());
	const glm::mat4 MVP = _camera.projection() * _camera.view() * glm::scale(glm::translate(glm::mat4(1.0f), _camera.getCenter()), glm::vec3(0.015f*scale));
	const auto debugProgram = Resources::manager().getProgram("camera-center");
	const auto debugObject = Resources::manager().getMesh("sphere");
	glDisable(GL_BLEND);
	glEnable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glUseProgram(debugProgram->id());
	glBindVertexArray(debugObject.vId);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, debugObject.eId);
	
	glUniformMatrix4fv(debugProgram->uniform("mvp"), 1, GL_FALSE, &MVP[0][0]);
	glUniform2f(debugProgram->uniform("screenSize"), _config.screenResolution[0], _config.screenResolution[1]);
	glDrawElements(GL_TRIANGLES, debugObject.count, GL_UNSIGNED_INT, (void*)0);
	
	
	
	glBindVertexArray(0);
	glUseProgram(0);
	glEnable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);
	
	checkGLError();
}

void Renderer::loadAge(const std::string & path){
	Log::Info() << "Should load " << path << std::endl;
	Resources::manager().reset();
	_age.reset(new Age(path));
	// A Uru human is around 4/5 units in height apparently.
	_camera.setCenter(_age->getDefaultLinkingPoint());
	//_maxLayer = std::max(_maxLayer, _age->maxLayer());
}
void Renderer::update(){
	if(Input::manager().resized()){
		resize((int)Input::manager().size()[0], (int)Input::manager().size()[1]);
	}
	_camera.update();
}

void Renderer::physics(double fullTime, double frameTime){
	_camera.physics(frameTime);
}

/// Clean function
void Renderer::clean() const {
	Resources::manager().reset();
}

/// Handle screen resizing
void Renderer::resize(int width, int height){
	updateResolution(width, height);
	// Update the projection aspect ratio.
	_camera.ratio(_renderResolution[0] / _renderResolution[1]);
}



void Renderer::updateResolution(int width, int height){
	_config.screenResolution[0] = float(width > 0 ? width : 1);
	_config.screenResolution[1] = float(height > 0 ? height : 1);
	// Same aspect ratio as the display resolution
	_renderResolution = (_config.internalVerticalResolution/_config.screenResolution[1]) * _config.screenResolution;
}

void Renderer::defaultGLSetup(){
	// Default GL setup.
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glDepthFunc(GL_LEQUAL);
	glFrontFace(GL_CCW);
	glCullFace(GL_BACK);
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
}



