$stdout.sync = true

require "bundler/rubygems_integration"

integration = Bundler.rubygems
puts integration.is_a?(Bundler::RubygemsIntegration)
puts integration.gem_dir == Gem.dir
puts integration.gem_bindir == Gem.bindir
puts integration.default_path.is_a?(Array)
puts Bundler.respond_to?(:rubygems)
