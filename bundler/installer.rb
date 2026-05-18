require "bundler"

module Bundler
  class Installer
    attr_reader :root, :definition

    def self.install(root, definition, options = {})
      new(root, definition, options)
    end

    def initialize(root, definition, options = {})
      @root = root
      @definition = definition
      @options = options
    end

    def options
      @options
    end

    def run
      true
    end
  end
end
