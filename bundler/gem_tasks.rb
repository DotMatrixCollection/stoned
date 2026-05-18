require "bundler"
require File.join(BUNDLER_RB_DIR, "bundler", "gem_helper")

# bundler/gem_tasks — provides Rake tasks for gem development
# This is a stub; full Rake integration is not implemented.
# The important thing is that require "bundler/gem_tasks" doesn't raise.

module Bundler
  GemTasks = GemHelper
end
