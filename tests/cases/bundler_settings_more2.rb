require "bundler/settings"

s = Bundler::Settings.new("/tmp/stoned_bundle_settings_more")
s.set_local("path", "vendor/bundle")
p s["path"]
