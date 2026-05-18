require "bundler"

module Bundler
  class RubyDsl
    attr_reader :versions

    def initialize
      @versions = []
    end

    def ruby(*requirements)
      @versions = requirements.map(&:to_s)
    end
  end
end
