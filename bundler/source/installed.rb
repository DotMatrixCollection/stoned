require "bundler"

module Bundler
  module Source
    class Installed
      def initialize(specs = [])
        @specs = specs
      end

      def specs
        @specs
      end

      def to_s
        "installed gems"
      end
    end
  end
end
