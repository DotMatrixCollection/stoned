$stdout.sync = true

require "bundler/gem_helper"
puts Bundler::GemHelper.respond_to?(:install_tasks)
puts Bundler::GemHelper.install_tasks.nil?

helper = Bundler::GemHelper.new
puts helper.is_a?(Bundler::GemHelper)
puts helper.install.nil?

require "bundler/gem_tasks"
puts Bundler::GemTasks == Bundler::GemHelper
