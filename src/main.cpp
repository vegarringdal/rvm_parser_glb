#include "RvmParser.h"
#include <argumentum/argparse-h.h>

int main(int argc, char **argv)
{

    std::string filename_including_path;
    std::string output_path;
    uint8_t export_level;
    uint8_t remove_duplicate_positions_precision;
    float tolerance;
    float meshopt_threshold;
    float meshopt_error;
    float meshopt_target_error;
    bool remove_elements_without_primitives;
    bool remove_duplicate_positions;

    auto parser = argumentum::argument_parser{};
    auto params = parser.params();
    parser.config().program(argv[0]).description("Rvm To Merged GLB (1 mesh per color)");

    params.add_parameter(filename_including_path, "--input", "-i")
        .nargs(1)
        .required(true)
        .help("rvm fileinput --input ./somefile.rvm");

    params.add_parameter(output_path, "--output", "-o")
        .nargs(1)
        .absent("./exports/")
        .help("Output folder, will create folder if it does not exist. Default is ./exports/");

    params.add_parameter(export_level, "--level", "-l")
        .nargs(1)
        .absent(0)
        .help("Level to split into files. Default is 0 (site)");

    params.add_parameter(remove_elements_without_primitives, "--remove-empty", "-r")
        .nargs(1)
        .absent(1)
        .help("Removes elements without primitives. This is enabled by default. To disable use -r 0");

    params.add_parameter(remove_duplicate_positions, "--cleanup-position", "-d")
        .nargs(1)
        .absent(1)
        .help("Removes duplicate positions. This is enabled by default. If you want to calculate vertex normals later, then you want this set to 0, to disable use -d 0");

    params.add_parameter(remove_duplicate_positions_precision, "--cleanup-precision", "-p")
        .nargs(1)
        .absent(3)
        .help("Precision to use when cleaning up duplicate positions, this also runs meshopt on mesh, default is 3");

    params.add_parameter(meshopt_threshold, "--meshopt-threshold", "-m")
        .nargs(1)
        .absent(0.75f)
        .help("meshopt threshold, default is 0.75f, only used when cleanup-position is active");
    
    params.add_parameter(meshopt_target_error, "--meshopt-target-error", "-e")
        .nargs(1)
        .absent(0.f)
        .help("meshopt target_error, default is 0.f, only used when cleanup-position is active");


    params.add_parameter(tolerance, "--tolerance", "-t")
        .nargs(1)
        .absent(0.01)

        .help("Tolerance to be used in triangulation, default is 0.01");

    if (!parser.parse_args(argc, argv, 1))
    {
        return 1;
    }

    RvmParser rvmParser;
    return rvmParser.read_file(
        filename_including_path, 
        output_path, 
        export_level, 
        remove_elements_without_primitives, 
        remove_duplicate_positions, 
        remove_duplicate_positions_precision, 
        tolerance,
        meshopt_threshold,
        meshopt_target_error
    );
}
