#pragma once

#include <gcc-plugin.h>
#include <context.h>
#include <basic-block.h>
#include <rtl.h>
#include <tree-pass.h>
#include <tree.h>

#include <stdio.h>


class my_PLUGIN : public rtl_opt_pass{
	public:
		my_PLUGIN(gcc::context *ctxt, struct plugin_argument *arguments, int argcounter);
//		bool gate(function *fun);
		unsigned int execute(function *fun);
	private:
		const char* findArgumentValue(const char* key);

		int argc;
		struct plugin_argument *args;
};
