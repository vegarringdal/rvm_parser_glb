#include <cstdint>
#include <string>
#include <fstream>
#include "RvmParser.h"
#include <iostream>

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION       // todo,  move, prob should not be loaded more than 1 time ?
#define STB_IMAGE_WRITE_IMPLEMENTATION // todo, move, prob should not be loaded more than 1 time ?// todo, move, prob should not be loaded more than 1 time ?
#include "tinygltf/tiny_gltf.h"

void rotate_z_up_to_y_up(float &x, float &y, float &z)
{
    float newY = z;
    float newZ = -y;
    y = newY;
    z = newZ;
}

// helper if we clean up duplicate positions
std::string generate_position_id(float x1, float x2, float x3, int precision = 3)
{
    // Scale factor based on desired precision
    int scaleFactor = static_cast<int>(std::pow(10, precision));

    // using precision of 2, will give scale factor of 100
    // 326.676605 will be 326 68 with scale factor of 100 /precision 2
    // 326.676605 will be 326 677 with scale factor of 1000 /precision 3
    // so I prob want to add this as a option to parser, having 3 as default ? its just for viewing..

    // Scale and round the floats to the desired precision
    int ix1 = static_cast<int>(std::round(x1 * scaleFactor));
    int ix2 = static_cast<int>(std::round(x2 * scaleFactor));
    int ix3 = static_cast<int>(std::round(x3 * scaleFactor));

    // Preallocate string memory for better performance
    std::string positionID;
    positionID.reserve(3 * (precision + 1) + 2); // +2 for the commas

    // Manually concatenate the integers with commas
    positionID.append(std::to_string(ix1)).append(",").append(std::to_string(ix2)).append(",").append(std::to_string(ix3));

    return positionID;
}

std::string RvmParser::generate_glb_from_current_root(std::vector<uint32_t> &colors)
{

    tinygltf::TinyGLTF gltf;
    tinygltf::Model m;

    tinygltf::Scene scene;
    tinygltf::Value::Object meta;

    tinygltf::Asset asset;
    asset.version = "2.0";
    asset.generator = "tinygltf";

    tinygltf::Value::Object asset_extras;
    asset_extras["web3dversion"] = tinygltf::Value(2);
    asset.extras = tinygltf::Value(asset_extras);

    m.asset = asset;

    tinygltf::Buffer buffer;

    size_t bufferSize = 0;
    uint32_t node_count = 0;
    uint32_t accessor_count = 0;
    uint32_t index_count = 0;

    // --------------------------------------------------------
    // next part will remove all elements without primititives
    // --------------------------------------------------------

    if (pRemoveElementsWithoutPrimitives)
    {

        auto removing_elements_without_primitives = true;
        auto cleanup_count = 0;
        while (removing_elements_without_primitives)
        {
            std::set<uint32_t> parents;
            for (auto &pair : p_nodes)
            {
                MetaNode &node = pair.second;
                parents.insert(node.parentId);
            }

            auto count = 0;
            std::vector<uint32_t> to_delete;
            for (auto &pair : p_nodes)
            {

                uint32_t id = pair.first;
                MetaNode &node = pair.second;

                auto c = 0;
                for (auto &tri : node.primitives)
                {
                    c += tri.triangulation->triangles_n * 3;
                    c += tri.triangulation->vertices_n * 3;
                }

                auto search = parents.find(id);
                if (c == 0 && search == parents.end())
                {

                    to_delete.push_back(id);
                    count++;
                    cleanup_count++;
                }
            }

            for (auto id : to_delete)
            {
                p_nodes.erase(id);
            }

            if (count == 0)
            {
                removing_elements_without_primitives = false;
            }
        }

        std::cout << "Removed empty elements: " << cleanup_count << "\n";
    }

    // --------------------------------------------------------
    // loop colors and generate file with 1 merged mesh per color
    // --------------------------------------------------------

    for (const auto &color : colors)
    {

        // next part will update drawranges for each item

        int32_t start = 0;
        uint32_t triangle_size = 0;
        uint32_t verticies_size = 0;
        for (auto &pair : p_nodes)
        {

            uint32_t id = pair.first;
            MetaNode &node = pair.second;

            if (color != node.colorWithAlpha || node.primitives.size() == 0)
            {
                continue;
            }

            node.start = start;
            for (auto &tri : node.primitives)
            {
                auto count = tri.triangulation->triangles_n * 3;
                node.count += count;
                triangle_size += count;
                verticies_size += tri.triangulation->vertices_n * 3;
                start += count;
            }
        }

        if (triangle_size == 0 && verticies_size == 0)
        {
            // if we dont have any primitives, we skip it
            continue;
        }

        // --------------------------------------------------------
        // next part will generate indices/position arrays
        // and collect bounding boxes for each merged mesh
        // --------------------------------------------------------

        std::cout << "Adding mesh with color id:" << color << std::endl;

        // collect buffer sizes
        size_t indicesSize = triangle_size * sizeof(uint32_t);
        size_t positionsSize = verticies_size * sizeof(float);
        size_t newBufferSize = bufferSize + indicesSize + positionsSize;

        // create temp memory space
        uint32_t *indicies = (uint32_t *)malloc(indicesSize);
        float *positions = (float *)malloc(positionsSize);

        uint32_t inCount = 0;
        uint32_t c = 0;
        uint32_t triCount = 0;
        uint32_t max_index = 0;
        uint32_t offset = 0;

        float min_x = 0;
        float min_y = 0;
        float min_z = 0;

        float max_x = 0;
        float max_y = 0;
        float max_z = 0;

        bool update_max_min = true;

        for (auto &pair : p_nodes)
        {

            uint32_t id = pair.first;
            MetaNode &node = pair.second;
            if (color != node.colorWithAlpha || node.primitives.size() == 0)
            {
                continue;
            }

            for (auto &tri : node.primitives)
            {
                auto ti = tri.triangulation->triangles_n * 3;
                for (int i = 0; i < ti; i++)
                {
                    auto t = tri.triangulation->indices[i];
                    if (t + offset > max_index)
                    {
                        max_index = t + offset;
                    }
                    auto in = (uint32_t)tri.triangulation->indices[i];
                    indicies[c] = in + offset;

                    c += 1;
                }
                offset = max_index + 1;

                for (int i = 0; i < tri.triangulation->vertices_n; i++)
                {
                    int u = triCount * 3;
                    int y = i * 3;

                    positions[u] = (float)tri.triangulation->vertices[y];
                    positions[u + 1] = (float)tri.triangulation->vertices[y + 1];
                    positions[u + 2] = (float)tri.triangulation->vertices[y + 2];

                    // rvm files are Z up by default, but glb files use Y
                    // so we want to rotate it
                    rotate_z_up_to_y_up(positions[u], positions[u + 1], positions[u + 2]);

                    if (update_max_min)
                    {

                        // first round, set init values and update for each round

                        min_x = positions[u];
                        min_y = positions[u + 1];
                        min_z = positions[u + 2];

                        max_x = positions[u];
                        max_y = positions[u + 1];
                        max_z = positions[u + 2];
                        update_max_min = false;
                    }

                    // get bounding box
                    if (min_x > positions[u])
                    {
                        min_x = positions[u];
                    }

                    if (min_y > positions[u + 1])
                    {
                        min_y = positions[u + 1];
                    }

                    if (min_z > positions[u + 2])
                    {
                        min_z = positions[u + 2];
                    }

                    if (max_x < positions[u])
                    {
                        max_x = positions[u];
                    }

                    if (max_y < positions[u + 1])
                    {
                        max_y = positions[u + 1];
                    }

                    if (max_z < positions[u + 2])
                    {
                        max_z = positions[u + 2];
                    }

                    triCount++;
                }
            }
        }

        // --------------------------------------------------------
        // next part will clean up position if enabled or use full set
        // --------------------------------------------------------

        std::unordered_map<std::string, uint32_t> position_index_map;
        std::vector<uint32_t> new_indecies;
        std::vector<float> new_positions;
        uint32_t indexCounter = 0;

        if (pRemoveDuplicatePositions)
        {

            for (auto i = 0; i < triangle_size; i++)
            {
                auto x = indicies[i] * 3;

                auto x1 = positions[x];
                auto x2 = positions[x + 1];
                auto x3 = positions[x + 2];

                std::string position_id = generate_position_id(x1, x2, x3, pRemoveDuplicatePositionsPrecision);

                auto search = position_index_map.find(position_id);
                if (search != position_index_map.end())
                {
                    new_indecies.push_back(search->second);
                }
                else
                {
                    new_indecies.push_back(indexCounter);
                    position_index_map.insert_or_assign(position_id, indexCounter);
                    indexCounter = indexCounter + 1;
                    new_positions.push_back(x1);
                    new_positions.push_back(x2);
                    new_positions.push_back(x3);
                }
            }
        }
        else
        {
            // full set
            indexCounter = max_index + 1;
            new_indecies.insert(new_indecies.end(), indicies, indicies + indicesSize / 4);
            new_positions.insert(new_positions.end(), positions, positions + positionsSize / 4);
        }

        // --------------------------------------------------------
        // init tinyglft we need to support indecies and positions
        // --------------------------------------------------------

        tinygltf::Mesh mesh;
        tinygltf::Primitive primitive;
        tinygltf::Node node;

        tinygltf::BufferView bufferView1;
        tinygltf::BufferView bufferView2;
        tinygltf::Accessor accessor1;
        tinygltf::Accessor accessor2;
        tinygltf::Material mat;

        std::vector<double> baseColorFactor;
        baseColorFactor.push_back((1.f / 255.f) * ((color >> 16) & 0xff));
        baseColorFactor.push_back((1.f / 255.f) * ((color >> 8) & 0xff));
        baseColorFactor.push_back((1.f / 255.f) * ((color) & 0xff));
        baseColorFactor.push_back((1.f / 255.f) * ((color >> 24) & 0xff));
        if (baseColorFactor[3] != 1)
        {
            mat.alphaMode = "BLEND";
        }

        mat.pbrMetallicRoughness.baseColorFactor = baseColorFactor;
        mat.pbrMetallicRoughness.metallicFactor = 0.5;
        mat.pbrMetallicRoughness.roughnessFactor = 0.5;

        mat.doubleSided = true;

        // indecies
        bufferView1.buffer = 0;
        bufferView1.byteOffset = bufferSize;
        bufferView1.byteLength = new_indecies.size() * sizeof(uint32_t);
        bufferView1.target = TINYGLTF_TARGET_ELEMENT_ARRAY_BUFFER;

        // positions
        bufferView2.buffer = 0;
        bufferView2.byteOffset = bufferSize + new_indecies.size() * sizeof(uint32_t);
        bufferView2.byteLength = new_positions.size() * sizeof(float);
        bufferView2.target = TINYGLTF_TARGET_ARRAY_BUFFER;

        // Describe the layout of bufferView1, the indices of the vertices
        accessor1.bufferView = accessor_count;
        accessor1.byteOffset = 0;
        accessor1.componentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT;
        accessor1.count = new_indecies.size();
        accessor1.type = TINYGLTF_TYPE_SCALAR;
        accessor1.maxValues.push_back(indexCounter - 1);
        accessor1.minValues.push_back(0);
        accessor_count += 1;

        // Describe the layout of bufferView2, the vertices themself
        accessor2.bufferView = accessor_count;
        accessor2.byteOffset = 0;
        accessor2.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
        accessor2.count = new_positions.size() / 3;
        accessor2.type = TINYGLTF_TYPE_VEC3;
        accessor2.minValues = {min_x, min_y, min_z};
        accessor2.maxValues = {max_x, max_y, max_z};
        accessor_count += 1;

        // Build the mesh primitive and add it to the mesh
        primitive.indices = index_count;                    // The index of the accessor for the vertex indices
        primitive.attributes["POSITION"] = index_count + 1; // The index of the accessor for positions
        primitive.material = m.materials.size();
        primitive.mode = TINYGLTF_MODE_TRIANGLES;
        mesh.primitives.push_back(primitive);

        index_count += 2;

        node.mesh = node_count;
        node.name = std::string("node") + std::to_string(node_count).c_str();

        m.materials.push_back(mat);
        m.meshes.push_back(mesh);
        m.nodes.push_back(node);

        m.bufferViews.push_back(bufferView1);
        m.bufferViews.push_back(bufferView2);
        m.accessors.push_back(accessor1);
        m.accessors.push_back(accessor2);

        // extend main raw buffer and clean up
        bufferSize = bufferSize + (new_indecies.size() * sizeof(uint32_t)) + (new_positions.size() * sizeof(float));
        buffer.data.insert(buffer.data.end(), reinterpret_cast<char *>(new_indecies.data()), reinterpret_cast<char *>(new_indecies.data()) + (new_indecies.size() * sizeof(uint32_t)));
        buffer.data.insert(buffer.data.end(), reinterpret_cast<char *>(new_positions.data()), reinterpret_cast<char *>(new_positions.data()) + new_positions.size() * sizeof(float));

        free(indicies);
        free(positions);

        // --------------------------------------------------------
        // next part collects and add draw_ranges for this node
        // --------------------------------------------------------

        tinygltf::Value::Object record;
        for (const auto &pair : p_nodes)
        {
            uint32_t id = pair.first;
            const MetaNode &node = pair.second;

            if (color != node.colorWithAlpha || node.primitives.size() == 0)
            {
                continue;
            }

            tinygltf::Value::Array nodeObject;

            nodeObject.push_back(tinygltf::Value((int)node.start));
            nodeObject.push_back(tinygltf::Value((int)node.count));

            std::string attName = std::to_string(node.id).c_str();

            record[attName] = tinygltf::Value(nodeObject);
        }

        auto attName = std::string("draw_ranges_node") + std::to_string(node_count).c_str();
        meta[attName] = tinygltf::Value(record);

        node_count += 1;
    }

    m.buffers.push_back(buffer);

    // --------------------------------------------------------
    // next part generates the id hierarchy for all ids and adds scene to file
    // --------------------------------------------------------

    tinygltf::Value::Object record;
    for (const auto &pair : p_nodes)
    {
        uint32_t id = pair.first;
        const MetaNode &node = pair.second;

        tinygltf::Value::Array nodeObject;
        nodeObject.push_back(tinygltf::Value(node.name));

        std::string parent_id;
        if (node.parentId == 0)
        {
            parent_id = std::string("*").c_str();
        }
        else
        {
            parent_id = std::to_string(node.parentId).c_str();
        }

        nodeObject.push_back(tinygltf::Value(parent_id));

        std::string attName = std::to_string(node.id).c_str();
        record[attName] = tinygltf::Value(nodeObject);
    }

    meta["id_hierarchy"] = tinygltf::Value(record);
    scene.extras = tinygltf::Value(meta);
    scene.nodes.push_back(0);
    m.scenes.push_back(scene);

    // --------------------------------------------------------
    // next part generate file / directory
    // --------------------------------------------------------

    auto name = get_file_name() + ".glb";
    if (bufferSize > 0)
    {
        if (pOutputPath.length() > 0 && !std::filesystem::exists(pOutputPath))
        {
            std::filesystem::create_directories(pOutputPath);
            std::cout << "Directory created: " << pOutputPath << std::endl;
        }

        gltf.WriteGltfSceneToFile(&m, pOutputPath + name.c_str(),
                                  true,  // embedImages
                                  true,  // embedBuffers
                                  true,  // pretty print
                                  true); // write binary

        return name;
    }

    return "";
}
