$stdout.sync = true

require "bundler/rubygems_integration"

integration = Bundler.rubygems
puts integration.is_a?(Bundler::RubygemsIntegration)
puts integration.gem_dir == Gem.dir
puts integration.gem_bindir == Gem.bindir
puts integration.default_path.is_a?(Array)
puts integration.platforms[0] == "ruby"
puts integration.loaded_specs.is_a?(Hash)
puts integration.all_specs.is_a?(Array)
puts integration.installed_specs.is_a?(Array)
puts integration.find_name("definitely-missing").length
puts Bundler.respond_to?(:rubygems)
