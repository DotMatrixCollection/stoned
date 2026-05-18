$stdout.sync = true

require "bundler"
puts Bundler::VERSION
puts Bundler.respond_to?(:setup)
puts Bundler.respond_to?(:with_unbundled_env)

require "bundler/gem_tasks"
puts Bundler::GemTasks.is_a?(Module)
