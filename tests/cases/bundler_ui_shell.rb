$stdout.sync = true

require "bundler/ui/shell"

ui = Bundler::UI::Shell.new
puts ui.is_a?(Bundler::BundlerUI)
puts Bundler.ui.is_a?(Bundler::UI::Shell)
puts Bundler.ui == Bundler.ui
