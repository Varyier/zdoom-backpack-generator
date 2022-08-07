
#include <fstream>
#include <iostream>

#include "common.h"
#include "core.h"
#include "io.h"

using namespace BpkGen;


int main(int argc, char *argv[]) {
	const StringArray args_and_opts(argv, argv + argc);

	const std::string oname_ignore_template_label_errors("--ignore-template-label-errors");
	const std::string oname_decorate_template_file("--decorate-template-file");
	const std::string oname_sbarinfo_template_file("--sbarinfo-template-file");
	const std::string oname_addfile("-addfile");

	bool display_help = false;
	std::string decorate_template_file;
	std::string sbarinfo_template_file;
	bool ignore_template_label_errors = false;
	StringArray args;
	std::vector< std::pair<std::string, std::string> > additional_files;
	for(size_t i=1; i<args_and_opts.size(); i++) {
		const std::string& arg = args_and_opts[i];

		if(arg.empty()) {
			continue;
		}

		if(arg == "/?") {
			display_help = true;

		} else if(arg[0] == '-') {
			// option
			if(arg == oname_decorate_template_file || arg.find(oname_decorate_template_file + "=") == 0) {
				if(arg.size() <= oname_decorate_template_file.size() + 1) {
					std::cout << "Error: a value must be specified for option '" + oname_decorate_template_file + "'" << std::endl;
					return 1;
				}

				decorate_template_file = arg.substr(oname_decorate_template_file.size()+1);

			} else if(arg == oname_sbarinfo_template_file || arg.find(oname_sbarinfo_template_file + "=") == 0) {
				if(arg.size() <= oname_sbarinfo_template_file.size() + 1) {
					std::cout << "Error: a value must be specified for option '" + oname_sbarinfo_template_file + "'" << std::endl;
					return 1;
				}

				sbarinfo_template_file = arg.substr(oname_sbarinfo_template_file.size() + 1);

			} else if(arg == oname_addfile) {
				if((i+1) >= args_and_opts.size()) {
					std::cout << "Error: a file name must be specified after option '" + oname_addfile + "'" << std::endl;
					return 1;
				}

				const std::string& file = args_and_opts[i+1];
				const size_t pos = file.find_last_of("\\/");
				const std::string lump_name = (pos < file.size()) ? file.substr(pos+1) : file;
				if(!Io::WadWriter::IsValidLumpName(lump_name)) {
					std::cout << "Error: bad additional file name '" + args_and_opts[i+1] + "' - must be valid lump name" << std::endl;
					return 1;
				}

				additional_files.push_back(std::make_pair(file, lump_name));
				i++;

			} else if(arg == oname_ignore_template_label_errors || arg.find(oname_ignore_template_label_errors + "=") == 0) {
				if(arg.size() > oname_ignore_template_label_errors.size()) {
					const std::string value = arg.substr(oname_ignore_template_label_errors.size() + 1);
					if(value == "true" || value == "1") {
						ignore_template_label_errors = true;

					} else if(value == "false" || value == "0") {
						ignore_template_label_errors = false;

					} else {
						std::cout << "Error: bad value for option '" + oname_ignore_template_label_errors + "'" << std::endl;
						return 1;
					}

				} else {
					ignore_template_label_errors = true;
				}

			} else if(arg == "--help" || arg == "-h" || arg == "/?") {
				// display help
				display_help = true;

			} else {
				std::cout << "Error: unknown option - " + arg;
				return 1;
			}

		} else {
			// argument
			args.push_back(arg);
		}
	}

	if(args.empty() || display_help) {
		std::cout << "backpack-gen - allows to create multiple backpack items for ZDoom-based games. "
		             "The tool generates required actor definitions as a part of DECORATE lump, also generates required code for SBARINFO lump. "
		             "Resulting lumps are packed into the output WAD file." << std::endl;
		std::cout << "Usage:" << std::endl;
		std::cout << "  backpack-gen.exe <ammo-config-file-path> [<output-file-path>] [<options>]" << std::endl;
		std::cout << "Arguments:" << std::endl;
		std::cout << "  <ammo-config-file-path> - ammo config file path (required)" << std::endl;
		std::cout << "  <output-file-path> - output WAD file path (optional, default - 'backpacks.wad')" << std::endl;
		std::cout << "Options: " << std::endl;
		std::cout << "  -addfile <file-path> - add more files 'as is' to the output WAD file" << std::endl;
		std::cout << "  --decorate-template-file=<file-path> - path of DECORATE lump template file" << std::endl;
		std::cout << "  --sbarinfo-template-file=<file-path> - path of SBARINFO lump template file" << std::endl;
		std::cout << "  --ignore-template-label-errors - do not abort on errors related to labels in templates (missing, duplicated, extra, redundant, misplased, bad parameters or others)" << std::endl;
		std::cout << "  --help, -h or /? - display this message" << std::endl;

		if(args.empty()) {
			return 0;
		}
	}

	//
	// 1) Read ammo config data from file
	//

	Io::ConfigData ammo_config_data;
	{
		const std::string ammo_config_file_path = args[0];
		std::ifstream file_input;
		file_input.exceptions(std::ios_base::badbit | std::ios_base::failbit);
		try {
			file_input.open(ammo_config_file_path);

		} catch(std::exception& e) {
			std::cout << "Error opening ammo config file '" << ammo_config_file_path << "': " << e.what() << std::endl;
			return 1;
		}

		try {
			Io::ReadConfigFromStream(file_input, ammo_config_data);

		} catch(Exception& e) {
			std::cout << "Error reading ammo config file '" << ammo_config_file_path << "': " << e.what() << std::endl;
			file_input.close();
			return 1;
		}

		file_input.close();
	}

	//
	// 2) Transform ammo config data into the struct
	//

	AmmoConfig ammo_config;
	try {
		GetAmmoConfig(ammo_config_data, ammo_config);

	} catch(Exception& e) {
		std::cout << "Error building ammo config: " << e.what() << std::endl;
		return 1;
	}

	//
	// 3) Open WAD output stream
	//

	const std::string output_wad_file_path = (args.size() > 1) ? args[1] : "backpacks.wad";
	std::ofstream file_output;
	file_output.exceptions(std::ios_base::badbit | std::ios_base::failbit);
	try {
		file_output.open(output_wad_file_path, std::ios::binary);

	} catch(std::exception& e) {
		std::cout << "Error opening output WAD file '" << output_wad_file_path << "': " << e.what() << std::endl;
		return 1;
	}
	Io::WadWriter wad_writer(file_output);

	//
	// 4) if DECORATE template file is specified, then generate DECORATE and put it into the WAD
	//

	if(!decorate_template_file.empty()) {
		std::ifstream file_input;
		file_input.exceptions(std::ios_base::badbit | std::ios_base::failbit);
		try {
			file_input.open(decorate_template_file);

		} catch(std::exception& e) {
			std::cout << "Error opening DECORATE lump template file '" << decorate_template_file << "': " << e.what() << std::endl;
			return 1;
		}

		try {
			WriteDecorateLumpToStream(ammo_config, file_input, wad_writer.StartLumpWriting("DECORATE"),
									  ignore_template_label_errors, &std::cout);

		} catch(std::exception& e) {
			file_input.close();
			file_output.close();
			std::cout << "Error processing DECORATE lump template file: " << e.what() << std::endl;
			return 1;
		}

		file_input.close();
	}

	//
	// 5) if SBARINFO template file is specified, then generate SBARINFO and put it into the WAD
	//

	if(!sbarinfo_template_file.empty()) {
		std::ifstream file_input;
		file_input.exceptions(std::ios_base::badbit | std::ios_base::failbit);
		try {
			file_input.open(sbarinfo_template_file);

		} catch(std::exception& e) {
			std::cout << "Error opening SBARINFO lump template file '" << sbarinfo_template_file << "': " << e.what() << std::endl;
			return 1;
		}

		try {
			WriteSbarinfoLumpToStream(ammo_config, file_input, wad_writer.StartLumpWriting("SBARINFO"),
									  ignore_template_label_errors, &std::cout);

		} catch(std::exception& e) {
			file_input.close();
			file_output.close();
			std::cout << "Error processing SBARINFO lump template file: " << e.what() << std::endl;
			return 1;
		}

		file_input.close();
	}

	//
	// 6) Put specified additional files into the WAD as is
	//
	
	for(size_t i=0; i<additional_files.size(); i++) {
		const std::string& file_path = additional_files[i].first;
		const std::string& lump_name = additional_files[i].second;
		std::ifstream file_input;
		file_input.exceptions(std::ios_base::badbit | std::ios_base::failbit);

		try {
			file_input.open(file_path);

		} catch(std::exception& e) {
			std::cout << "Error opening additional file '" << file_path << "': " << e.what() << std::endl;
			return 1;
		}

		try {
			Io::OutStream& out_stream = wad_writer.StartLumpWriting(lump_name);
			for(Io::InStreamWithBuffer file_input_with_buffer(file_input); !file_input_with_buffer.IsEndOfStream(); file_input_with_buffer.MoveToNextChar()) {
				out_stream << file_input_with_buffer.GetCurChar();
			}

		} catch(std::exception& e) {
			file_input.close();
			file_output.close();
			std::cout << "Error writing additional file '" + file_path + "' to WAD: " << e.what() << std::endl;
			return 1;
		}

		file_input.close();
	}

	//
	// 7) Finish writing WAD file
	//

	try {
		wad_writer.FinishWriting();

	} catch(Exception& e) {
		file_output.close();
		std::cout << "Error writing WAD file '" + output_wad_file_path + "': " << e.what() << std::endl;
		return 1;
	}

	file_output.close();
	std::cout << "Wad file created successfully!" << std::endl;
	return 0;
}
