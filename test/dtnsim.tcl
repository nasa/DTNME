#
#    Copyright 2004-2006 Intel Corporation
# 
#    Licensed under the Apache License, Version 2.0 (the "License");
#    you may not use this file except in compliance with the License.
#    You may obtain a copy of the License at
# 
#        http://www.apache.org/licenses/LICENSE-2.0
# 
#    Unless required by applicable law or agreed to in writing, software
#    distributed under the License is distributed on an "AS IS" BASIS,
#    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#    See the License for the specific language governing permissions and
#    limitations under the License.
#

test::name dtnsim-basic
net::default_num_nodes 1

set conf_path ""
foreach {var val} $opt(opts) {
    if {$var == "-c"} {
	set conf_path $val
    } else {
        error "unknown test option $var"
    }
}

if {$conf_path == ""} {
    error "must specify simulator config file"
}

if {[file exists $base_test_dir/sim/conf/$conf_path]} {
    set conf_path $base_test_dir/sim/conf/$conf_path
}

if {![file exists $conf_path]} {
    error "simulator conf file $conf_path does not exist"
}

test::script {
    testlog "Running simulator"
    exec sim/dtnsim -c $conf_path <@stdin >@stdout 2>@stderr
    
    testlog "Test success!"
}
