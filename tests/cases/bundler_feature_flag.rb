$stdout.sync = true

require "bundler/feature_flag"

old_path = ENV["BUNDLE_PATH"]
ENV["BUNDLE_PATH"] = nil
flag1 = Bundler.feature_flag
puts flag1.path_relative_to_cwd? == false
puts flag1.plugins? == false
puts flag1.default_install_uses_path? == false

ENV["BUNDLE_PATH"] = "vendor/bundle"
flag2 = Bundler::FeatureFlag.new(Bundler::Settings.new)
puts flag2.default_install_uses_path?

ENV["BUNDLE_PATH"] = old_path
