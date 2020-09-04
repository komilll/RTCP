#include "LightSettings.h"
#include <iostream>
#include <fstream>
#include <sstream>

void LightSettings::AddLight(Light light)
{
	m_lights.push_back(light);
}

void LightSettings::RemoveLight(int lightIndex)
{
	m_lights.erase(m_lights.begin() + lightIndex);
}

void LightSettings::SaveLightToFile()
{
	if (std::ofstream file{ lightDataPath, std::ofstream::out })
	{
		std::string data = "";
		for (const auto& light : m_lights)
		{
			file << std::to_string(static_cast<int>(light.type)) + "\n";
			file << light.position.x << " " << light.position.y << " " << light.position.z << "\n";
			file << light.rotation.x << " " << light.rotation.y << " " << light.rotation.z << "\n";
			file << light.color.x << " " << light.color.y << " " << light.color.z << " " << light.color.w << "\n";
		}
	}
}

void LightSettings::LoadLightFromFile()
{
	if (std::ifstream file{ lightDataPath, std::ifstream::in })
	{
		std::string line;
		int i = 0;
		while (std::getline(file, line))
		{
			std::istringstream iss(line);
			std::vector<std::string> results(std::istream_iterator<std::string>{iss},
				std::istream_iterator<std::string>());

			if (i == 0) {
				m_lights.push_back({});
			}
			int el = static_cast<int>(m_lights.size()) - 1;

			// Type
			if (i == 0)
			{
				m_lights[el].type = static_cast<LightType>(std::stoi(results[0]));
			}
			// Position
			else if (i == 1)
			{
				m_lights[el].position = XMFLOAT3{ std::stof(results[0]), std::stof(results[1]), std::stof(results[2]) };
			}
			// Rotation
			else if (i == 2)
			{
				m_lights[el].rotation = XMFLOAT3{ std::stof(results[0]), std::stof(results[1]), std::stof(results[2]) };
			}
			// Color
			else if (i == 3)
			{
				m_lights[el].color = XMFLOAT4{ std::stof(results[0]), std::stof(results[1]), std::stof(results[2]), std::stof(results[3]) };
			}

			i++;
			i %= 4;
		}
	}
}
