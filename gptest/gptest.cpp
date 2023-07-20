// This is the first gcc header to be included
#include "gcc-plugin.h"
#include "plugin-version.h"
#include "my_plugin.h"

#include <iostream>

const struct pass_data my_PLUGIN_pass_data =
{
    .type = RTL_PASS,
    .name = "myPlugin",
    .optinfo_flags = OPTGROUP_NONE,
    .tv_id = TV_NONE,
    .properties_required = PROP_rtl, // | PROP_cfglayout),
    .properties_provided = 0,
    .properties_destroyed = 0,
    .todo_flags_start = 0,
    .todo_flags_finish = 0,
};

my_PLUGIN::my_PLUGIN(gcc::context *ctxt, struct plugin_argument *arguments, int argcounter)
                : rtl_opt_pass(my_PLUGIN_pass_data, ctxt)
{
    argc = argcounter;          // number of arguments
    args = arguments;           // array containing arrguments (key,value)
}

unsigned int my_PLUGIN::execute(function *fun)
{
  // 1) Find the name of the function
  char* funName = (char*)IDENTIFIER_POINTER (DECL_NAME (current_function_decl) );
  std::cerr << "execute on " << funName << "\n";
  return 0;
}

// We must assert that this plugin is GPL compatible
int plugin_is_GPL_compatible;

int plugin_init (struct plugin_name_args *plugin_info, struct plugin_gcc_version *version)
{
    // We check the current gcc loading this plugin against the gcc we used to
    // created this plugin
    if (!plugin_default_version_check (version, &gcc_version))
    {
        std::cerr << "This GCC plugin is for version " << GCCPLUGIN_VERSION_MAJOR << "." << GCCPLUGIN_VERSION_MINOR << "\n";
	return 1;
    }

    // Let's print all the information given to this plugin!

    std::cerr << "Plugin info\n===========\n";
    std::cerr << "Base name: " << plugin_info->base_name << "\n";
    std::cerr << "Full name: " << plugin_info->full_name << "\n";
    std::cerr << "Number of arguments of this plugin:" << plugin_info->argc << "\n";

    for (int i = 0; i < plugin_info->argc; i++)
    {
        std::cerr << "Argument " << i << ": Key: " << plugin_info->argv[i].key << ". Value: " << plugin_info->argv[i].value<< "\n";
    }

    std::cerr << "\nVersion info\n============\n";
    std::cerr << "Base version: " << version->basever << "\n";
    std::cerr << "Date stamp: " << version->datestamp << "\n";
    std::cerr << "Dev phase: " << version->devphase << "\n";
    std::cerr << "Revision: " << version->devphase << "\n";
    std::cerr << "Configuration arguments: " << version->configuration_arguments << "\n";
    std::cerr << "\n";

    struct register_pass_info pass;
    pass.pass = new my_PLUGIN(g, plugin_info->argv, plugin_info->argc);
    pass.reference_pass_name = "final";
    pass.ref_pass_instance_number = 1;
    pass.pos_op = PASS_POS_INSERT_BEFORE;

    register_callback("myPlugin", PLUGIN_PASS_MANAGER_SETUP, NULL, &pass);
    std::cerr << "Plugin successfully initialized\n";

    return 0;
}