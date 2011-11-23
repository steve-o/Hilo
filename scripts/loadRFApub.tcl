#
# Load RFA Vision Publisher (if not already loaded)
#

set mod_id RFAVelocityPublisher-Module;
set mod_path {%s\Config\RFAVelocityProvider.xml}; # [VHAYU_INSTALLER_ADJUSTED]

set mod_list [tb_pf_listmodules]; # get the list of loaded modules
foreach {mod} $mod_list {
    if {[string first $mod_id $mod] == 0} { # found the module id, it's already loaded
        return $mod_id;
    }
}

return [tb_pf_loadmodule $mod_path]; # load the module
