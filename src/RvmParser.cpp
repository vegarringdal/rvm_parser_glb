#include <cstdint>
#include <unordered_map>
#include <string>
#include <vector>
#include <array>
#include <set>
#include <fstream>
#include <iostream>
#include <chrono>
#define _FILE_OFFSET_BITS 64
#include <sys/stat.h>
#include "Arena.h"
#include "RvmParser.h"
#include "Geometry.h"
#include "LinAlgOps.h"
#include "Tessellator.h"
#include "TriangulationFactory.h"
#include "ColorStore.h"
#include "../libs/rapidjson/include/document.h"
#include "../libs/rapidjson/include/stringbuffer.h"
#include "../libs/rapidjson/include/writer.h"
#include "md5.h"

std::string RvmParser::get_file_name()
{

    std::string::iterator it;
    std::string s = current_root_name.c_str();
    for (it = s.begin(); it < s.end(); ++it)
    {
        switch (*it)
        {
        case '/':
        case '\\':
        case ':':
        case '?':
        case '"':
        case '<':
        case '>':
        case '|':
            *it = '$';
        }
    }

    return s;
}

int RvmParser::read_file(
    std::string filename,
    std::string output_path,
    uint8_t export_level,
    bool remove_elements_without_primitives,
    bool remove_duplicate_positions,
    uint8_t remove_duplicate_positions_precision,
    float tolerance,
    float meshopt_threshold,
    float meshopt_target_error)
{
    p_export_level = export_level;
    p_remove_elements_without_primitives = remove_elements_without_primitives;
    p_remove_duplicate_positions = remove_duplicate_positions;
    p_remove_duplicate_positions_precision = remove_duplicate_positions_precision;
    p_output_path = output_path;
    p_tolerance = tolerance;
    p_meshopt_threshold = meshopt_threshold;
    p_meshopt_target_error = meshopt_target_error;

    auto start = std::chrono::high_resolution_clock::now();

    file_stream.open(filename, std::ios_base::binary);

    p_buffer_total_length = get_file_size(filename);
    if (p_buffer_total_length == 0)
    {
        std::cout << "file not found or size is 0" << std::endl;
        return 1;
    }

    std::cout << "File found, starting to read" << std::endl;

    p_buffer = new uint8_t[p_buffer_size];

    read_next_chunk();

    auto result = start_reading();
    delete p_buffer;

    generate_status_file();

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = end - start;
    std::cout << "Execution time: " << duration.count() << " seconds" << std::endl;

    return 0;
}

uint32_t RvmParser::read_next_chunk()
{

    file_stream.read(reinterpret_cast<char *>(p_buffer), p_buffer_size);
    p_buffer_input_length = file_stream.gcount();

    return p_buffer_input_length;
}

uint32_t RvmParser::get_file_size(std::string filename)
{
    struct stat stat_buf;
    int rc = stat(filename.c_str(), &stat_buf);
    return rc == 0 ? stat_buf.st_size : -1;
}

void RvmParser::store_last_node()
{

    if (p_node_count_id > 0)
    {

        std::vector<NodePrim> insu;
        std::vector<NodePrim> obst;
        std::vector<NodePrim> prim;

        // need to loop primitives, so we can duplicate node if there is insulation/obstruction primitives

        for (auto &tri : p_node.primitives)
        {
            if (tri.type == Geometry::Type::Primitive)
            {
                prim.push_back(tri);
            }

            if (tri.type == Geometry::Type::Insulation)
            {
                insu.push_back(tri);
            }

            if (tri.type == Geometry::Type::Obstruction)
            {
                obst.push_back(tri);
            }
        }

        if (p_node.primitives.size() == 0)
        {

            MetaNode node;
            node.id = p_node_count_id;
            node.parent_id = p_node.parent_id;
            node.name = p_node.name;
            node.start = p_node.start;
            node.count = p_node.count;
            node.opacity = p_node.opacity;
            node.color_with_alpha = 0;
            node.material_id = 0;
            p_nodes.insert_or_assign(p_node_count_id, std::move(node));
        }

        if (prim.size() > 0)
        {

            uint8_t alpha = static_cast<uint8_t>((p_node.opacity * 255) / 100);
            uint32_t color_with_alpha = (alpha << 24) | (p_node.material_id & 0xFFFFFF);

            p_site_color_with_alpha.insert(color_with_alpha);

            MetaNode node;
            node.id = p_node.id;
            node.parent_id = p_node.parent_id;
            node.name = p_node.name;
            node.start = p_node.start;
            node.count = p_node.count;
            node.opacity = p_node.opacity;
            node.color_with_alpha = color_with_alpha;
            node.material_id = p_node.material_id;
            node.primitives = prim;
            p_nodes.insert_or_assign(p_node_count_id, std::move(node));
        }

        // TODO: should be option if you want to include this..
        if (insu.size() > 0)
        {
            auto first = insu.front();

            uint8_t alpha = static_cast<uint8_t>((first.opacity * 255) / 100);
            uint32_t color_with_alpha = (alpha << 24) | (p_node.material_id & 0xFFFFFF);

            p_site_color_with_alpha.insert(color_with_alpha);

            if (prim.size() > 0)
            {
                p_node_count_id += 1;
            }
            MetaNode node;
            node.id = p_node_count_id;
            node.parent_id = p_node.parent_id;
            node.name = p_node.name + std::string("(INSU)"); // TODO: this needs to be a option
            node.start = p_node.start;
            node.count = p_node.count;
            node.opacity = p_node.opacity;
            node.color_with_alpha = color_with_alpha;
            node.material_id = p_node.material_id;
            node.primitives = insu;
            p_nodes.insert_or_assign(p_node_count_id, std::move(node));
        }

        // TODO: should be option if you want to include this..
        if (obst.size() > 0)
        {

            auto first = obst.front();
            uint8_t alpha = static_cast<uint8_t>((first.opacity * 255) / 100);
            uint32_t color_with_alpha = (alpha << 24) | (p_node.material_id & 0xFFFFFF);

            p_site_color_with_alpha.insert(color_with_alpha);

            if (prim.size() > 0 || insu.size() > 0)
            {
                p_node_count_id += 1;
            }

            MetaNode node;
            node.id = p_node_count_id;
            node.parent_id = p_node.parent_id;
            node.name = p_node.name + std::string("(OBST)"); // TODO: this needs to be a option
            node.start = p_node.start;
            node.count = p_node.count;
            node.opacity = p_node.opacity;
            node.color_with_alpha = color_with_alpha;
            node.material_id = p_node.material_id;
            node.primitives = obst;
            p_nodes.insert_or_assign(p_node_count_id, std::move(node));
        }
    }
}

int RvmParser::start_reading()
{

    char chunk_name[5] = {0, 0, 0, 0, 0};
    p_next_chunk = 0;
    p_index_total = 0;

    ////////////////////////
    // HEAD
    ////////////////////////

    p_next_chunk = parse_chunk(chunk_name);
    if (chunk_id("HEAD") != chunk_id(chunk_name))
    {
        p_collected_errors.push_back("Did not find HEAD element");
        std::cout << "Did not find HEAD element" << std::endl;
        return 1;
    }

    HeadBlock headerBlock = parse_head_block();

    if (p_index != p_next_chunk)
    {
        p_collected_errors.push_back("Uexpected chunk found on HEAD");
        std::cout << "Uexpected chunk found on HEAD" << std::endl;
        std::cout << "Expected:" << p_next_chunk << " at:" << static_cast<uint32_t>(p_index_total) << std::endl;
        return 2;
    }

    // save for later
    p_header = headerBlock;

    ////////////////////////
    // MODL
    ////////////////////////

    p_next_chunk = parse_chunk(chunk_name);
    if (chunk_id("MODL") != chunk_id(chunk_name))
    {

        p_collected_errors.push_back("Missing MODL element");
        return 1; // Missing modl
    }

    ModlBlock modlBlock = parse_modl_block();

    if (p_index != p_next_chunk)
    {
        p_collected_errors.push_back("Uexpected chunk found on MODL");
        std::cout << "Uexpected chunk found on MODL" << std::endl;
        std::cout << "Expected:" << p_next_chunk << " at:" << static_cast<uint32_t>(p_index_total) << std::endl;
        return 2;
    }

    ////////////////////////
    // ALL ELEMENTS
    ////////////////////////

    while (p_index < p_buffer_total_length)
    {

        p_next_chunk = parse_chunk(chunk_name);

        // verify offset

        switch (chunk_id(chunk_name))
        {

        ////////////////////////
        // CNTB
        ////////////////////////
        case chunk_id("CNTB"):
        {

            CntbBlock cntb = parse_cntb_block();

            // quickfix for cntb version 4 offset i dont know what is..
            if (cntb.version == 4)
            {
                if (static_cast<uint32_t>(p_index_total) < p_next_chunk)
                {
                    std::cout << "fixing, Expected:" << p_next_chunk << " at:" << static_cast<uint32_t>(p_index_total) << std::endl;
                    std::cout << cntb.name << std::endl;
                    while (p_next_chunk != static_cast<uint32_t>(p_index_total))
                    {
                        read_uint8();
                    }
                }
            }

            if (static_cast<uint32_t>(p_index_total) != p_next_chunk)
            {
                p_collected_errors.push_back("Uexpected chunk found on CNTB, at root:" + current_root_name);
                std::cout << "Uexpected chunk found on CNTB" << current_root_name << std::endl;
                std::cout << "Expected:" << p_next_chunk << " at:" << static_cast<uint32_t>(p_index_total) << std::endl;
                return 2;
            }

            if (p_level < p_export_level)
            {
                p_level += 1;
                break;
            }

            if (p_level == p_export_level)
            {

                MD5 new_ctx;
                // overwrite old context when we start new group
                p_md5 = new_ctx;

                if (arenaTriangulation == nullptr)
                {
                    arenaTriangulation = new Arena();
                }
                else
                {
                    p_nodes.clear();
                    arenaTriangulation->clear();
                }

                current_root_name = cntb.name;

                // we reset IDs per root lvl, since we do export per root lvl
                p_node_count_id = 0;
                p_parent_node_id = 0;
                p_parent_stack.clear();
                p_parent_stack.push_back(p_node_count_id);
            }
            else
            {
                store_last_node();
            }

            p_node_count_id += 1;

            // reset our shared node
            p_node.id = p_node_count_id;
            p_node.parent_id = p_parent_stack.back();
            p_node.name = cntb.name;
            p_node.start = 0;
            p_node.count = 0;
            p_node.version = cntb.version;
            p_node.color_with_alpha = 0;
            p_node.opacity = cntb.opacity; // might be changed
            p_node.material_id = cntb.material;
            p_node.primitives.clear();

            p_parent_stack.push_back(p_node_count_id);

            // todo append to map
            p_level += 1;
        }
        break;
        ////////////////////////
        // COLR
        ////////////////////////
        case chunk_id("COLR"):
        {
            ColrBlock colrBlock = parse_colr_block();

            if (static_cast<uint32_t>(p_index_total) != p_next_chunk)
            {
                p_collected_errors.push_back("Uexpected chunk found on COLR");
                // in theory this could be the last element, so we could allow it...
                std::cout << "Uexpected chunk found on COLR" << current_root_name << std::endl;
                std::cout << "Expected:" << p_next_chunk << " at:" << static_cast<uint32_t>(p_index_total) << std::endl;
                return 2;
            }

            std::cout << "Using default colors, not support for COLR block" << std::endl;
        }
        break;
        ////////////////////////
        // END:
        ////////////////////////
        case chunk_id("END:"):
        {

            read_uint32_be();
            if (static_cast<uint32_t>(p_index_total) != p_next_chunk)
            {
                p_collected_errors.push_back("Uexpected chunk found on END:");
                std::cout << "Uexpected chunk found on END:" << current_root_name << std::endl;
                std::cout << "Expected:" << p_next_chunk << " at:" << static_cast<uint32_t>(p_index_total) << std::endl;
                return 2;
            }

            return 0;
        }

        break;
        ////////////////////////
        // CNTE
        ////////////////////////
        case chunk_id("CNTE"):

            // skip version
            read_uint32_be();

            if (static_cast<uint32_t>(p_index_total) != p_next_chunk)
            {
                p_collected_errors.push_back("Uexpected chunk found on CNTE");
                std::cout << "Uexpected chunk found on CNTE" << current_root_name << std::endl;
                std::cout << "Expected:" << p_next_chunk << " at:" << static_cast<uint32_t>(p_index_total) << std::endl;
                return 2;
            }

            if (p_level < p_export_level)
            {
                p_level -= 1;
                break;
            }

            p_parent_node_id = p_parent_stack.back();
            p_parent_stack.pop_back();

            p_level -= 1;

            if (p_level == p_export_level)
            {

                store_last_node();
                p_md5.finalize();
                std::vector<uint32_t> colors;
                for (uint32_t color_id : p_site_color_with_alpha)
                {
                    colors.push_back(color_id);
                }

                std::string root_md5 = p_md5.hexdigest();

                std::cout << "Generating root: " << current_root_name << ", MD5:" << root_md5 << '\n';
                auto file_name = generate_glb_from_current_root(colors);

                if (file_name.length() > 0)
                {
                    // store current root level for later, so we can make a json file with this info
                    FileMeta file_meta;
                    file_meta.md5 = root_md5;
                    file_meta.root_name = current_root_name;
                    file_meta.file_name = file_name;

                    if (auto search = p_filemeta_map.find(current_root_name); search != p_filemeta_map.end())
                    {
                        // just incase, should we have a error array ?
                        p_collected_errors.push_back("Root name aready exsist: " + current_root_name);
                        std::cout << "Root name aready exsist: " << current_root_name << ", MD5:" << root_md5 << '\n';
                    }

                    p_filemeta_map.insert_or_assign(current_root_name, file_meta);
                }
                else
                {
                    std::cout << "Root name had no triangles, skipping: " << current_root_name << ", MD5:" << root_md5 << '\n';
                }
            }

            break;
        ////////////////////////
        // PRIM/OBST/INSU
        ////////////////////////
        case chunk_id("PRIM"):
            [[fallthrough]];
        case chunk_id("OBST"):
            [[fallthrough]];
        case chunk_id("INSU"):

            p_prim_count_id += 1;

            parse_prim_block(chunk_id(chunk_name));

            // quickfix for cntb version 4 offset i dont know what is..
            if (p_node.version == 4)
            {
                if (static_cast<uint32_t>(p_index_total) < p_next_chunk)
                {
                    std::cout << "fixing, Expected:" << p_next_chunk << " at:" << static_cast<uint32_t>(p_index_total) << std::endl;
                    while (p_next_chunk != static_cast<uint32_t>(p_index_total))
                    {
                        read_uint8();
                    }
                }
            }

            // if we cant fix its a error..
            if (static_cast<uint32_t>(p_index_total) != p_next_chunk)
            {
                p_collected_errors.push_back("Uexpected chunk found on PRIM/OBST/INSU: " + current_root_name);
                std::cout << "Uexpected chunk found on PRIM/OBST/INSU:" << current_root_name << std::endl;
                std::cout << "Expected:" << p_next_chunk << " at:" << static_cast<uint32_t>(p_index_total) << std::endl;
                return 2;
            }

            break;

        default:
            p_collected_errors.push_back("unknown element found:" + chunk_name[0] + chunk_name[1] + chunk_name[2] + chunk_name[3] + chunk_name[4] );
            std::cout << "unknown chuck found" << chunk_name << std::endl;
            return 3;
        }
    }

    return 0;
};
