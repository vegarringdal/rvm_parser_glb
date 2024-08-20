#include <cstdint>
#include <string>
#include <fstream>
#include <filesystem>
#include "RvmParser.h"
#include "rapidjson/include/document.h"
#include "rapidjson/include/stringbuffer.h"
#include "rapidjson/include/writer.h"

/**
 * Generates status file for current rvm file
 * This will have name of root element, md5 for that root element and file path
 * Also have rvm header info
 * Useful if you need to update user files etc
 */
void RvmParser::generate_status_file()
{

    
    using namespace rapidjson;

    Document document;
    document.SetObject();
    Document::AllocatorType &allocator = document.GetAllocator();

    Value models(kArrayType);
    for (const auto &pair : p_filemeta_map)
    {
        auto key = pair.first;
        auto &node = pair.second;

        Value nodeObject(kObjectType);
        nodeObject.AddMember("root_name", Value().SetString(node.root_name.c_str(), node.root_name.length(), allocator), allocator);
        nodeObject.AddMember("md5", Value().SetString(node.md5.c_str(), node.md5.length(), allocator), allocator);
        nodeObject.AddMember("file_name", Value().SetString(node.file_name.c_str(), node.file_name.length(), allocator), allocator);
        models.PushBack(nodeObject, allocator);
    }
    document.AddMember("models", models, allocator);

    Value warnings(kArrayType);
    for (const auto &warning : p_collected_errors)
    {
        models.PushBack(Value().SetString(warning.c_str(), warning.length(), allocator), allocator);
    }
    document.AddMember("warnings", warnings, allocator);

    Value header_object(kObjectType);
    header_object.AddMember("date", Value().SetString(p_header.date.c_str(), p_header.date.length(), allocator), allocator);
    header_object.AddMember("encoding", Value().SetString(p_header.encoding.c_str(), p_header.encoding.length(), allocator), allocator);
    header_object.AddMember("info", Value().SetString(p_header.info.c_str(), p_header.info.length(), allocator), allocator);
    header_object.AddMember("note", Value().SetString(p_header.note.c_str(), p_header.note.length(), allocator), allocator);
    header_object.AddMember("user", Value().SetString(p_header.user.c_str(), p_header.user.length(), allocator), allocator);
    header_object.AddMember("version", p_header.version, allocator);
    document.AddMember("header", header_object, allocator);

    StringBuffer buffer;
    Writer<StringBuffer> writer(buffer);
    document.Accept(writer);

    // should I use original filename + status_file instead?
    std::string temp_filename = p_output_path + "status_file.json";

    if (p_output_path.length() > 0 && !std::filesystem::exists(p_output_path)) {
        std::filesystem::create_directories(p_output_path);
        std::cout << "Directory created: " << p_output_path << std::endl;
    }

    std::ofstream file_write;
    file_write.open(temp_filename, std::ios::out | std::ios::binary | std::ios::trunc);
    if (file_write.is_open()) {

        file_write.write((char *)buffer.GetString(), buffer.GetSize()); // saves the array into the file
        file_write.close();
        std::cout << "File created: " << temp_filename << std::endl;
    } else {
        std::cerr << "Failed writing to file: " << temp_filename << std::endl;
    }
}
