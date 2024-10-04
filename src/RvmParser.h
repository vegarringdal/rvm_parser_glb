#pragma once
#include <cstdint>
#include <unordered_map>
#include <string>
#include <vector>
#include <fstream>
#include <set>
#include "Arena.h"
#include "Geometry.h"
#include "ColorStore.h"
#include "md5.h"

void rotate_z_up_to_y_up(float &x, float &y, float &z);

struct NodePrim
{
    uint8_t opacity;
    Geometry::Type type;
    Triangulation *triangulation;
};

struct MetaNode
{
    uint32_t id;
    uint32_t parent_id;
    std::string name;
    uint32_t material_id;
    uint32_t start;
    uint32_t count;
    uint8_t opacity;
    uint8_t version;
    uint32_t color_with_alpha;
    std::vector<NodePrim> primitives;
};

struct NodeBox3
{
    float X;
    float Y;
    float Z;
    float x;
    float y;
    float z;
};

struct HeadBlock
{
    uint32_t version;
    std::string info;
    std::string note;
    std::string date;
    std::string user;
    std::string encoding;
};

struct ModlBlock
{
    uint32_t version;
    std::string project;
    std::string name;
};

struct ColrBlock
{
    uint32_t version;
    uint32_t index;
    float color[3];
};

struct FileMeta
{
    std::string root_name;
    std::string file_name;
    std::string md5;
    // might want some more here later
    // bbox ?
    // indcies/vertex size ?
    // bsphere ?
};

struct CntbBlock
{
    uint32_t version;
    std::string name;
    float translation[3];
    uint32_t material;
    float opacity;
};

enum struct FileType : uint8_t
{
    complete = 0,
    standard_index = 1,
    standard_position = 2,
    standard_json = 3,
};

constexpr uint32_t chunk_id(const char *str)
{
    return str[3] << 24 | str[2] << 16 | str[1] << 8 | str[0];
}

class RvmParser
{

public:
    RvmParser() = default;
    int read_file(
        std::string filename,
        std::string output_path,
        uint8_t export_level,
        bool remove_elements_without_primitives,
        bool remove_duplicate_positions,
        uint8_t remove_duplicate_positions_precision,
        float tolerance,
        float meshopt_threshold,
        float meshopt_target_error,
        bool is_dry_run);

private:
    std::ifstream file_stream_color_search;
    std::ifstream file_stream;
    uint32_t p_index_total = 0;
    uint32_t p_next_chunk = 0;

    MD5 p_md5;

    // 0 = site/first CNTB lvl
    // 1 = zone
    // by setting this you will generate files based on this
    uint8_t p_export_level = 0;
    bool p_remove_elements_without_primitives = false;
    bool p_remove_duplicate_positions = false;
    uint8_t p_remove_duplicate_positions_precision = 3;
    std::string p_output_path = "";
    float p_tolerance = 0.01;
    float p_meshopt_threshold = 0.f;
    float p_meshopt_target_error = 0.f;
    bool p_is_dry_run = false;

    // vars for loopin buffer
    uint32_t p_index = 0;
    uint32_t p_level = 0;
    std::string current_root_name;

    uint16_t p_buffer_size = 1024;
    uint8_t *p_buffer;
    uint32_t p_buffer_input_length;
    uint32_t p_buffer_total_length;

    Arena *arenaTriangulation = nullptr;

    HeadBlock p_header;
    std::unordered_map<std::string, FileMeta> p_filemeta_map;
    std::vector<std::string> p_collected_errors;

    // id of nodes within file /or three
    uint32_t p_node_count_id = 0;
    MetaNode p_node;
    uint32_t p_prim_count_id = 0;
    uint32_t p_parent_node_id = 0;
    std::vector<uint32_t> p_parent_stack = {0};

    ColorStore p_color_store;

    // key here is the p_node_count_id
    std::unordered_map<uint32_t, MetaNode> p_nodes;
    std::set<uint32_t> p_site_color_with_alpha;

    uint8_t read_uint8();

    uint32_t read_uint32_be();

    void store_last_node();

    float read_float32_be();

    std::string read_string();

    uint32_t parse_chunk(char *chunk_name);

    HeadBlock parse_head_block();

    ModlBlock parse_modl_block();

    ColrBlock parse_colr_block();

    CntbBlock parse_cntb_block();

    void parse_prim_block(uint32_t chunk_name_id);

    int start_reading();

    uint32_t get_file_size(std::string filename);

    uint32_t read_next_chunk();

    std::string get_file_name();
    std::string generate_glb_from_current_root(std::vector<uint32_t> &colors);
    void generate_status_file();
};
