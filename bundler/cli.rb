require "bundler"

module Bundler
  class CLI
    attr_reader :args

    def self.start(args = ARGV)
      new(args)
    end

    def initialize(args = [])
      @args = args
    end

    def install
      :install
    end

    def exec
      :exec
    end
  end
end
