#include <cstdint>
#include <string>
#include <fstream>
#include "RvmParser.h"
#include <iostream>
#include "meshoptimizer-0.21/src/meshoptimizer.h"

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION       // todo,  move, prob should not be loaded more than 1 time ?
#define STB_IMAGE_WRITE_IMPLEMENTATION // todo, move, prob should not be loaded more than 1 time ?// todo, move, prob should not be loaded more than 1 time ?
#include "tinygltf/tiny_gltf.h"

void update_bbox(bbox3 &b, float min_x, float min_y, float min_z, float max_x, float max_y, float max_z)
{
    // Update min values if they are not set or the new value is smaller
    if (!std::isnan(min_x) && min_x < b.min_x)
        b.min_x = min_x;
    if (!std::isnan(min_y) && min_y < b.min_y)
        b.min_y = min_y;
    if (!std::isnan(min_z) && min_z < b.min_z)
        b.min_z = min_z;

    // Update max values if they are not set or the new value is larger
    if (!std::isnan(max_x) && max_x > b.max_x)
        b.max_x = max_x;
    if (!std::isnan(max_y) && max_y > b.max_y)
        b.max_y = max_y;
    if (!std::isnan(max_z) && max_z > b.max_z)
        b.max_z = max_z;
}

void rotate_z_up_to_y_up(float &x, float &y, float &z)
{
    float new_y = z;
    float new_z = -y;
    y = new_y;
    z = new_z;
}

// helper if we clean up duplicate positions
std::string generate_position_id(float x1, float x2, float x3, int precision = 3)
{
    // Scale factor based on desired precision
    int scale_factor = static_cast<int>(std::pow(10, precision));

    // using precision of 2, will give scale factor of 100
    // 326.676605 will be 326 68 with scale factor of 100 /precision 2
    // 326.676605 will be 326 677 with scale factor of 1000 /precision 3
    // so I prob want to add this as a option to parser, having 3 as default ? its just for viewing..

    // Scale and round the floats to the desired precision
    int ix1 = static_cast<int>(std::round(x1 * scale_factor));
    int ix2 = static_cast<int>(std::round(x2 * scale_factor));
    int ix3 = static_cast<int>(std::round(x3 * scale_factor));

    // Preallocate string memory for better performance
    std::string position_id;
    position_id.reserve(3 * (precision + 1) + 2); // +2 for the commas

    // genrerate unique key
    position_id.append(std::to_string(ix1)).append(",").append(std::to_string(ix2)).append(",").append(std::to_string(ix3));

    return position_id;
}

std::vector<uint32_t> cleanDegenerateTriangles(
    const uint32_t *indices,
    size_t index_count,
    const float *vertex_positions,
    size_t vertex_count,
    bool logErrors = false)
{
    std::vector<uint32_t> cleaned;
    const float epsilon = 1e-8f;

    uint32_t count_degenerate_triangle_duplicate = 0;
    uint32_t count_degenerate_triangle_overlapping = 0;
    uint32_t count_degenerate_triangle_zero = 0;

    auto getPosition = [&](uint32_t i) -> std::array<float, 3>
    {
        return {
            vertex_positions[i * 3],
            vertex_positions[i * 3 + 1],
            vertex_positions[i * 3 + 2]};
    };

    auto cross = [](const std::array<float, 3> &a, const std::array<float, 3> &b) -> std::array<float, 3>
    {
        return {
            a[1] * b[2] - a[2] * b[1],
            a[2] * b[0] - a[0] * b[2],
            a[0] * b[1] - a[1] * b[0]};
    };

    auto length = [](const std::array<float, 3> &v) -> float
    {
        return std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    };

    for (size_t i = 0; i + 2 < index_count; i += 3)
    {
        uint32_t a = indices[i];
        uint32_t b = indices[i + 1];
        uint32_t c = indices[i + 2];

        if (a == b || b == c || c == a)
        {
            if (logErrors)
                count_degenerate_triangle_duplicate = count_degenerate_triangle_duplicate + 1;
            continue;
        }

        auto p0 = getPosition(a);
        auto p1 = getPosition(b);
        auto p2 = getPosition(c);

        bool samePosition =
            (p0 == p1) || (p1 == p2) || (p2 == p0);

        if (samePosition)
        {
            if (logErrors)
                count_degenerate_triangle_overlapping = count_degenerate_triangle_overlapping + 1;
            continue;
        }

        std::array<float, 3> ab = {p1[0] - p0[0], p1[1] - p0[1], p1[2] - p0[2]};
        std::array<float, 3> ac = {p2[0] - p0[0], p2[1] - p0[1], p2[2] - p0[2]};
        auto normal = cross(ab, ac);
        float area = 0.5f * length(normal);

        if (area < epsilon)
        {
            if (logErrors)
                count_degenerate_triangle_zero = count_degenerate_triangle_zero + 1;
            continue;
        }

        cleaned.push_back(a);
        cleaned.push_back(b);
        cleaned.push_back(c);
    }

    if (logErrors && (count_degenerate_triangle_duplicate || count_degenerate_triangle_overlapping || count_degenerate_triangle_zero))
    {
        std::cerr << "Degenerate triangle count:\n";
        std::cerr << "Duplicates:" << count_degenerate_triangle_duplicate << "\n";
        std::cerr << "Overlapping:" << count_degenerate_triangle_overlapping << "\n";
        std::cerr << "Zero:" << count_degenerate_triangle_zero << "\n";
    }

    return cleaned;
}

std::string RvmParser::generate_glb_from_current_root(std::vector<uint32_t> &colors, bbox3 &bbox)
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

    size_t buffer_size = 0;
    uint32_t node_count = 0;
    uint32_t accessor_count = 0;
    uint32_t index_count = 0;

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

            if (color != node.color_with_alpha || node.primitives.size() == 0)
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
        // next part will remove all elements without primititives
        // --------------------------------------------------------

        // --------------------------------------------------------
        // next part will remove all elements without primititives
        // --------------------------------------------------------

        if (p_remove_elements_without_primitives)
        {

            auto removing_elements_without_primitives = true;
            auto cleanup_count = 0;
            while (removing_elements_without_primitives)
            {
                std::set<uint32_t> parents;
                for (auto &pair : p_nodes)
                {
                    MetaNode &node = pair.second;
                    parents.insert(node.parent_id);
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
        // next part will generate indices/position arrays
        // and collect bounding boxes for each merged mesh
        // --------------------------------------------------------

        std::cout << "Adding mesh with color id:" << color << std::endl;

        // collect buffer sizes
        size_t indices_size = triangle_size * sizeof(uint32_t);
        size_t positions_size = verticies_size * sizeof(float);
        size_t new_buffer_size = buffer_size + indices_size + positions_size;

        // create temp memory space
        uint32_t *indicies = (uint32_t *)malloc(indices_size);
        float *positions = (float *)malloc(positions_size);

        uint32_t indecies_count = 0;
        uint32_t c = 0;
        uint32_t triangle_count = 0;
        uint32_t max_index = 0;
        uint32_t offset = 0;

        float min_x = 0;
        float min_y = 0;
        float min_z = 0;

        float max_x = 0;
        float max_y = 0;
        float max_z = 0;

        bool min_max_first_loop = true;

        for (auto &pair : p_nodes)
        {

            uint32_t id = pair.first;
            MetaNode &node = pair.second;
            if (color != node.color_with_alpha || node.primitives.size() == 0)
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
                    int u = triangle_count * 3;
                    int y = i * 3;

                    positions[u] = (float)tri.triangulation->vertices[y];
                    positions[u + 1] = (float)tri.triangulation->vertices[y + 1];
                    positions[u + 2] = (float)tri.triangulation->vertices[y + 2];

                    // rvm files are Z up by default, but glb files use Y
                    // so we want to rotate it
                    rotate_z_up_to_y_up(positions[u], positions[u + 1], positions[u + 2]);

                    if (min_max_first_loop)
                    {

                        // first round, set init values and update for each round

                        min_x = positions[u];
                        min_y = positions[u + 1];
                        min_z = positions[u + 2];

                        max_x = positions[u];
                        max_y = positions[u + 1];
                        max_z = positions[u + 2];

                        min_max_first_loop = false;
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

                    triangle_count++;
                }
            }
        }

        // --------------------------------------------------------
        // next part will clean up position if enabled or use full set
        // --------------------------------------------------------

        std::vector<uint32_t> new_indecies;
        std::vector<float> new_positions;
        uint32_t index_counter = 0;

        if (p_remove_duplicate_positions)
        {

            for (auto &pair : p_nodes)
            {

                uint32_t id = pair.first;
                MetaNode &node = pair.second;

                if (color != node.color_with_alpha || node.primitives.size() == 0)
                {
                    continue;
                }

                std::unordered_map<std::string, uint32_t> tmp_position_index_map;
                std::vector<uint32_t> temp_indecies;
                std::vector<float> temp_positions;
                uint32_t temp_index_counter = 0;

                for (auto i = node.start; i < node.start + node.count; i++)
                {
                    auto x = indicies[i] * 3;

                    // this just need to be unique
                    auto x1 = positions[x];
                    auto x2 = positions[x + 1];
                    auto x3 = positions[x + 2];
                    std::string position_id = generate_position_id(x1, x2, x3, p_remove_duplicate_positions_precision);

                    auto search = tmp_position_index_map.find(position_id);
                    if (search != tmp_position_index_map.end())
                    {
                        temp_indecies.push_back(search->second);
                    }
                    else
                    {
                        temp_indecies.push_back(temp_index_counter);
                        tmp_position_index_map[position_id] = temp_index_counter;
                        temp_index_counter++;
                        temp_positions.push_back(positions[x]);
                        temp_positions.push_back(positions[x + 1]);
                        temp_positions.push_back(positions[x + 2]);
                    }
                }

                float threshold = p_meshopt_threshold;
                size_t target_index_count = size_t(temp_indecies.size() * threshold);
                float target_error = p_meshopt_target_error;
                float lod_error = 0.f;
                std::vector<unsigned int> lod(temp_indecies.size());
                std::unordered_map<std::string, uint32_t> position_index_map;

                lod.resize(
                    meshopt_simplify(
                        &lod[0],
                        &temp_indecies[0],
                        temp_indecies.size(),
                        &temp_positions[0],
                        temp_positions.size() / 3,
                        12,
                        target_index_count,
                        target_error,
                        meshopt_SimplifyLockBorder, //(1) meshopt_SimplifyErrorAbsolute,  (4)
                        &lod_error));

                node.start = static_cast<uint32_t>(new_indecies.size());

                auto cleanedLod = cleanDegenerateTriangles(&lod[0], lod.size(), &temp_positions[0], temp_positions.size() / 3);

                for (auto i = 0; i < cleanedLod.size(); i++)
                {
                    auto x = cleanedLod[i] * 3;
                    auto x1 = temp_positions[x];
                    auto x2 = temp_positions[x + 1];
                    auto x3 = temp_positions[x + 2];
                    std::string position_id = generate_position_id(x1, x2, x3, p_remove_duplicate_positions_precision);

                    auto search = position_index_map.find(position_id);
                    if (search != position_index_map.end())
                    {
                        new_indecies.push_back(search->second);
                    }
                    else
                    {
                        new_indecies.push_back(index_counter);
                        position_index_map[position_id] = index_counter;
                        index_counter++;
                        new_positions.push_back(temp_positions[x]);
                        new_positions.push_back(temp_positions[x + 1]);
                        new_positions.push_back(temp_positions[x + 2]);
                    }
                }

                node.count = static_cast<uint32_t>(new_indecies.size()) - node.start;
            }
        }
        else
        {
            // full set
            index_counter = max_index + 1;
            new_indecies.insert(new_indecies.end(), indicies, indicies + indices_size / 4);
            new_positions.insert(new_positions.end(), positions, positions + positions_size / 4);
        }

        free(indicies);
        free(positions);

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

        std::vector<double> base_color;
        base_color.push_back((1.f / 255.f) * ((color >> 16) & 0xff));
        base_color.push_back((1.f / 255.f) * ((color >> 8) & 0xff));
        base_color.push_back((1.f / 255.f) * ((color) & 0xff));
        base_color.push_back((1.f / 255.f) * ((color >> 24) & 0xff));
        if (base_color[3] != 1)
        {
            mat.alphaMode = "BLEND";
            base_color[3] = 1.0 - base_color[3];
        }

        mat.pbrMetallicRoughness.baseColorFactor = base_color;
        mat.pbrMetallicRoughness.metallicFactor = 0.0;
        mat.pbrMetallicRoughness.roughnessFactor = 1.0;

        mat.doubleSided = true;

        // indecies
        bufferView1.buffer = 0;
        bufferView1.byteOffset = buffer_size;
        bufferView1.byteLength = new_indecies.size() * sizeof(uint32_t);
        bufferView1.target = TINYGLTF_TARGET_ELEMENT_ARRAY_BUFFER;

        // positions
        bufferView2.buffer = 0;
        bufferView2.byteOffset = buffer_size + new_indecies.size() * sizeof(uint32_t);
        bufferView2.byteLength = new_positions.size() * sizeof(float);
        bufferView2.target = TINYGLTF_TARGET_ARRAY_BUFFER;

        // Describe the layout of bufferView1, the indices of the vertices
        accessor1.bufferView = accessor_count;
        accessor1.byteOffset = 0;
        accessor1.componentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT;
        accessor1.count = new_indecies.size();
        accessor1.type = TINYGLTF_TYPE_SCALAR;
        accessor1.maxValues.push_back(index_counter - 1);
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

        update_bbox(bbox, min_x, min_y, min_z, max_x, max_y, max_z);

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
        scene.nodes.push_back(node_count);

        m.bufferViews.push_back(bufferView1);
        m.bufferViews.push_back(bufferView2);
        m.accessors.push_back(accessor1);
        m.accessors.push_back(accessor2);

        // extend main raw buffer and clean up
        buffer_size = buffer_size + (new_indecies.size() * sizeof(uint32_t)) + (new_positions.size() * sizeof(float));
        buffer.data.insert(buffer.data.end(), reinterpret_cast<char *>(new_indecies.data()), reinterpret_cast<char *>(new_indecies.data()) + (new_indecies.size() * sizeof(uint32_t)));
        buffer.data.insert(buffer.data.end(), reinterpret_cast<char *>(new_positions.data()), reinterpret_cast<char *>(new_positions.data()) + new_positions.size() * sizeof(float));

        // --------------------------------------------------------
        // next part collects and add draw_ranges for this node
        // --------------------------------------------------------

        tinygltf::Value::Object record;
        for (const auto &pair : p_nodes)
        {
            uint32_t id = pair.first;
            const MetaNode &node = pair.second;

            if (color != node.color_with_alpha || node.primitives.size() == 0 || node.count == 0)
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
        if (node.parent_id == 0)
        {
            parent_id = std::string("*").c_str();
        }
        else
        {
            parent_id = std::to_string(node.parent_id).c_str();
        }

        nodeObject.push_back(tinygltf::Value(parent_id));

        std::string attName = std::to_string(node.id).c_str();
        record[attName] = tinygltf::Value(nodeObject);
    }

    meta["id_hierarchy"] = tinygltf::Value(record);
    scene.extras = tinygltf::Value(meta);

    m.scenes.push_back(scene);

    // --------------------------------------------------------
    // next part generate file / directory
    // --------------------------------------------------------

    auto name = get_file_name() + ".glb";
    if (buffer_size > 0)
    {
        if (p_output_path.length() > 0 && !std::filesystem::exists(p_output_path))
        {
            std::filesystem::create_directories(p_output_path);
            std::cout << "Directory created: " << p_output_path << std::endl;
        }

        gltf.WriteGltfSceneToFile(&m, p_output_path + name.c_str(),
                                  true,  // embedImages
                                  true,  // embedBuffers
                                  true,  // pretty print
                                  true); // write binary

        return name;
    }

    return "";
}
