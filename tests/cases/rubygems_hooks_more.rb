require "rubygems"

p Gem.post_install_hooks.class
Gem.post_install_hooks << :hook
p Gem.post_install_hooks
p Gem.pre_uninstall_hooks
