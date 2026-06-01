require "bundler"

p Bundler::VERSION
p Bundler.respond_to?(:setup)
p Bundler.settings.class
